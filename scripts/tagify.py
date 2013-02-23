# Copyright Dave Abrahams 2013. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#!/usr/bin/env python

import sys,re

def read_refs():
    return dict(
        ('/'+src+'/', 
         dict(kind=kind, rev=int(rev), date=date, commits=int(commits), dest=dest)
     )
        for kind,rev,date,commits,src,dest in
        (line.split('\t') 
         for line in open('/Users/dave/src/ryppl/subconvert/doc/branches.txt')
         if '\t' in line))

branch_re = re.compile(
    r'\s*\[\s*(\d*)\s*:\s*(\d*)\s*\]\s*"([^"]+)"\s*:\s*"([^"]+)"\s*;\s*')

def run():
    refs = read_refs()
    
    state = 'outside'
    tags = None
    
    found_refs = set()

    for l in open("/Users/dave/src/ryppl/Boost2Git/repositories.txt"):
        if state == 'inside':
            stripped = l.strip()
            if stripped == '}':
                state = 'outside'
            elif stripped and not stripped.startswith('//'):
                m = branch_re.match(l)
                if not m:
                    raise RuntimeError, 'no match for branch_re in ' + repr(l)
                start, finish, src, dst = m.groups()
                found_refs.add(src)

                if dst != 'master' and refs.get(src,dict(kind='branch'))['kind'] == 'tag':
                    tags.append(l)
                    continue

        elif state == 'outside' and l.strip() == 'branches':
            state = 'branches'
        elif state == 'branches':
            if l.strip() == '{':
                state = 'inside'
                tags = []
            else:
                raise RuntimeError, 'unexpected syntax %r in state %s' % (l,state)
            
        sys.stdout.write(l)
        if state == 'outside' and tags:
            sys.stdout.write('  tags\n  {\n')
            sys.stdout.writelines(tags)
            sys.stdout.write('  }\n')
            tags = None

    print >>sys.stderr, 'subconvert refs not accounted for:'
    import pprint
    sys.stderr.write(
        pprint.pformat(dict(
            kv for kv in refs.items() 
            if kv[0] not in found_refs and kv[0] + 'boost/' not in found_refs)))

if __name__ == '__main__':
    run()
