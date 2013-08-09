# Boost2Git

This project converts an SVN repository into multiple Git
repositories, optionally registering each repository as a submodule in
some other Git repository.  It started out as KDE's
[svn2git](http://gitorious.org/svn2git/pages/Home) tool, but has been
almost completely rewritten, to the point where very little of the
original code remains.  

There were many reasons for our initial deviations from svn2git, but
the heart of the original program was still there until we discovered
it was
[producing nonsense results](https://groups.google.com/d/topic/boost-developers-archive/JSIQet6GBIM/discussion).
When we evaluated the core logic, it became clear that the svn2git
approach was insufficiently general to correctly handle our branch and
directory mapping structure.  Our rewrite requires C++11.

In the rewrite, we dropped several features of svn2git that aren't
needed for Boost, most notably incremental conversions.  The dropped
features could be brought back without too much difficulty, but
unless someone else takes over maintenance of this project, they are
unlikely to get addressed.  The
[issue tracker](https://github.com/ryppl/Boost2Git/issues?state=open)
is our record of what can or should still be done.

For any substantially large SVN-to-Git conversion + modularization
job, if you start with today's technology, some amount of coding will
be necessary.  Because it is quite general and fairly clean, Boost2Git
is probably a good starting point.

At the time of this writing, Boost is being
[continuously converted](http://jenkins.boost.org/job/Boost2Git/) into
[these](http://github.com/boostorg) Git repositories.

