/*
 *  Copyright (C) 2007  Thiago Macieira <thiago@kde.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Based on svn-fast-export by Chris Lee <clee@kde.org>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 * URL: git://repo.or.cz/fast-import.git http://repo.or.cz/w/fast-export.git
 */

// Apparently some builds of libsvn_repos/libsvn_ra_local (like the
// one that comes with MacOS 10.8) don't have svn_repos_open2, so we
// use the deprecated svn_repos_open instead.
#define SVN_DEPRECATED

#include "svn.hpp"
#include "svn_error.hpp"
#include "apr_init.hpp"
#include "apr_pool.hpp"

#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

AprInit apr_init;
AprPool svn::global_pool;

svn::svn(
    std::string const& repo_path,
    std::string const& authors_file_path)
    : repos(call(svn_repos_open, repo_path.c_str(), global_pool)),
      fs(svn_repos_fs(repos)),
      authors(authors_file_path)
{
}

svn::~svn()
{}

int svn::latest_revision() const
{
    return call(svn_fs_youngest_rev, fs, global_pool);
}

static std::string get_string(apr_hash_t *revprops, char const *key)
{
    std::string result;
    svn_string_t *str = (svn_string_t*) apr_hash_get(revprops, key, APR_HASH_KEY_STRING);
    if (str && !svn_string_isempty(str))
    {
        result.append(str->data, str->len);
    }
    return result;
}

svn::revision::revision(svn const& repo, int revnum)
    : pool(svn::global_pool.make_subpool())
    , fs_root(call(svn_fs_revision_root, repo.fs, revnum, pool))
    , revnum(revnum)
    , epoch(0)
{
    apr_hash_t *revprops = call(svn_fs_revision_proplist, repo.fs, revnum, pool);

    author = repo.authors[get_string(revprops, "svn:author")];
    if (author.empty())
        author = "nobody <nobody@localhost>";

    std::string svndate = get_string(revprops, "svn:date");
    if (!svndate.empty())
    {
        namespace dt = boost::date_time;
        namespace pt = boost::posix_time;
        pt::ptime ptime = dt::parse_delimited_time<pt::ptime>(svndate, 'T');
        static pt::ptime epoch_(boost::gregorian::date(1970, 1, 1));
        epoch = (ptime - epoch_).total_seconds();
    }

    log_message = get_string(revprops, "svn:log");
    if (log_message.empty())
        log_message = "** empty log message **";
}
