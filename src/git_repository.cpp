// Copyright Dave Abrahams 2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "git_repository.hpp"
#include "git_executable.hpp"
#include "log.hpp"
#include "flat_set_union.hpp"

#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <array>
#include <boost/range/adaptor/map.hpp>
#include <iomanip>

git_repository::git_repository(std::string const& git_dir)
    : git_dir(git_dir),
      created(ensure_existence(git_dir)),
      fast_import_(git_dir),
      super_module(nullptr),
      last_mark(0),
      current_ref(nullptr),
      prepared_to_close_commit(false)
{
}

bool git_repository::ensure_existence(std::string const& git_dir)
{
    namespace process = boost::process;
    namespace fs = boost::filesystem;
    using namespace process::initializers;
    
    if (fs::exists(git_dir))
        return true;

    // Create the new repository
    fs::create_directories(git_dir);
    std::array<std::string, 4> git_args = { git_executable(), "init", "--bare", "--quiet" };
    auto git_init = process::execute(
        run_exe(git_executable()),
        set_args(git_args), 
        start_in_dir(git_dir), 
        throw_on_error());
    wait_for_exit(git_init);
    return true;
}

void git_repository::set_super_module(
    git_repository* super_module, std::string const& submodule_path)
{
    if (super_module)
    {
        if (this->super_module != nullptr)
        {
            if (this->super_module != super_module)
                throw std::runtime_error("Conflicting super-module specifications");
            if (this->submodule_path != submodule_path)
                throw std::runtime_error("Conflicting submodule path declarations");
        }
        this->super_module = super_module;
        this->submodule_path = submodule_path;
    }
}

// This is the SHA1 of an empty tree.  We can use this to detect when
// branches are deleted.
std::string const empty_tree_sha("4b825dc642cb6eb9a060e54bf8d69288fbee4904");

void git_repository::prepare_to_close_commit()
{
    assert(current_ref);
    if (!current_ref->can_close())
        return;
    
    Log::trace() << "repository " << git_dir
                 << " preparing to close commit in ref " << current_ref->name << std::endl;

    auto subrefs = std::move(current_ref->stale_submodule_refs);
    subrefs |= current_ref->changed_submodule_refs;

    for (auto sr : subrefs)
    {
        assert(!sr->marks.empty());
        std::stringstream sha_prep;
        sha_prep << std::setfill('0') << std::setw(40) 
                 << std::prev(sr->marks.end())->second;
        fast_import() 
            << "M 160000 "
            << sha_prep.str()
            << " " << sr->repo->submodule_path << LF;
    }

    if (!subrefs.empty())
    {
        // Absorb any new changed submodule refs into the overall list of
        // submodule refs
        current_ref->submodule_refs |= subrefs;

        std::stringstream content;
        for (auto sr : current_ref->submodule_refs)
        {
            content << "[submodule \"" << sr->repo->name() << "\"]\n"
                    << "	path = " << sr->repo->submodule_path << "\n"
                    << "	url = http://github.com/boostorg/" << sr->repo->name() << ".git\n"
                    << "        fetchRecurseSubmodules = on-demand\n"
                ;
        }
        fast_import().filemodify_hdr(".gitmodules");
        fast_import().data(content.str().data(), content.str().size());
    }
    
    if (current_ref->gitattributes_outdated)
    {
        fast_import().filemodify_hdr(".gitattributes");
        fast_import().data(options.gitattributes.data(), options.gitattributes.size());
        current_ref->gitattributes_outdated = false;
    }

    // Send a fast-import "ls" command to the changed repository now;
    // responses will be read in a separate close_commit() pass over
    // all changed repos.  Hopefully this will prevent us from
    // blocking for each repo when multiple repositories are changed
    // in a single SVN revision.
    fast_import().send_ls("\"\"");
    prepared_to_close_commit = true;
}

// Close the current ref's commit.  Return true iff there are no more
// modified refs
bool git_repository::close_commit()
{
    assert(current_ref);
    if (!current_ref->can_close())
        return false;

    // Super-modules sometimes become ready to close just after their
    // submodules have closed, so we may not have prepared them for
    // closure when we arrive here; do it on-demand.
    if (!prepared_to_close_commit)
        prepare_to_close_commit();

    Log::trace() << "repository " << git_dir
                 << " closing commit in ref " << current_ref->name << std::endl;

    std::string new_sha;
    if (!options.dry_run)
    {
        // Read the response to the git-fast-import "ls" command sent earlier
        std::string response = fast_import().readline();
    
        if (response.size() < 41)
            Log::error() << "Unrecognized response \"" << response << "\" from ls in ref " 
                         << current_ref->name << std::endl;
        else
            new_sha = response.substr(response.size() - 41, response.size() - 1);
    }

    // Dispose of the commit if it didn't change anything in the tree
    if (!options.dry_run && new_sha == current_ref->head_tree_sha) 
    {
        Log::trace() << "Tree unchanged; resetting ref" << std::endl;
        assert(current_ref->marks.size() >= 2);
        current_ref->marks.erase(std::prev(current_ref->marks.end()));
        fast_import().reset(current_ref->name, std::prev(current_ref->marks.end())->second);
        // Also retract the modification from the super-module
        if (auto s = current_ref->super_module_ref)
            s->changed_submodule_refs.erase(current_ref);
    }
    else
    {
        current_ref->head_tree_sha = std::move(new_sha);
        if (auto s = current_ref->super_module_ref)
        {
            s->submodule_refs_written += 1;
            Log::trace() << "In repo " << super_module->name() << " " 
                         << s->submodule_refs_written << "/" << s->changed_submodule_refs.size() 
                         << " modified submodule refs written" << std::endl;
            assert(s->submodule_refs_written <= s->changed_submodule_refs.size());
        }
    }

    current_ref->stale_submodule_refs.clear();
    current_ref->changed_submodule_refs.clear();
    current_ref->submodule_refs_written = 0;

    // Done changing this ref
    modified_refs.erase(current_ref);
    current_ref = nullptr;
    prepared_to_close_commit = false;
    Log::trace() << modified_refs.size() << " modified refs remaining." << std::endl;
    return modified_refs.empty();
}

void git_repository::write_merges()
{
    assert(current_ref);
    for (auto const& kv : current_ref->pending_merges)
    {
        auto src_ref = kv.first;
        auto src_rev = kv.second;

        if (src_rev > current_ref->merged_revisions[src_ref])
        {
            auto p = src_ref->marks.upper_bound(src_rev);
            if (p == src_ref->marks.begin())
            {
                Log::warn() << "No commit found at or preceding the source of merge r" 
                            << src_rev << " in Git repo " << git_dir << " ref " 
                            << src_ref->name << std::endl;
                continue;
            }
            fast_import() << "merge :" << (--p)->second << LF;
            current_ref->merged_revisions[src_ref] = src_rev;
        }
    }
    current_ref->pending_merges.clear();
}

git_repository::ref* git_repository::open_commit(svn::revision const& rev)
{
    if (current_ref) // Commit is already open
        return current_ref;

    assert(!modified_refs.empty());

    current_ref = *std::prev(modified_refs.end());

    Log::trace() << "repository " << git_dir
                 << " opening commit in ref " << current_ref->name << std::endl;

    int mark = ++last_mark;
    current_ref->marks[rev.revnum] = mark;
    fast_import() << "# SVN revision " << rev.revnum << LF;
    fast_import().commit(current_ref->name, mark, rev.author, rev.epoch, rev.log_message);

    // Write any merges required in this ref
    write_merges();

    // Do any deletions required in this ref
    for (auto& p : current_ref->pending_deletions)
    {
        fast_import().filedelete(p);

        // Make sure we rewrite the refs of all submodules caught by
        // this delete.  The submodule repositories themselves don't
        // (necessarily) need an update.
        current_ref->stale_submodule_refs
            |= current_ref->submodule_refs | boost::adaptors::filtered(
                [&](ref const* r){ return r->repo->submodule_path.starts_with(p); });

        if (p.str().empty() && !options.gitattributes.empty())
            current_ref->gitattributes_outdated = true;
    }
    current_ref->pending_deletions.clear();

    return current_ref;
}

void git_repository::record_ancestor(
    ref* descendant, std::string const& src_ref_name, std::size_t revnum)
{
    // Don't bother recording merges from one branch into itself; that
    // ancestry is already going to be represented.
    if (src_ref_name != descendant->name)
    {
        auto src_ref = demand_ref(src_ref_name);

        // Update the latest source revision merged
        auto& merged_rev = descendant->pending_merges[src_ref];
        if (merged_rev < revnum)
            merged_rev = revnum;
    }
}

git_repository::ref* git_repository::modify_ref(std::string const& name, bool allow_discovery)
{
    auto r = demand_ref(name);
    bool already_modified = modified_refs.count(r);
    if (!already_modified)
    {
        if (!allow_discovery)
            return nullptr;

        Log::trace() << "In Git repo " << this->name() << ", marking " << r->name 
                     << " for modification" << std::endl;

        modified_refs.insert(r);

        if (super_module)
        {
            if (auto super_module_ref = super_module->modify_ref(name, allow_discovery))
            {
                Log::trace() << "Marking super-module " << super_module_ref->repo->name() 
                             << ", ref " << r->name << " for modification" << std::endl;
                super_module_ref->changed_submodule_refs.insert(r);
            }
        }
    }

    return r;
}

