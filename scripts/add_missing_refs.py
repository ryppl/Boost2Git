# Copyright Dave Abrahams 2013. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#!/usr/bin/env python

import sys,re

def read_subconvert_refs():
    return dict(
        ('/'+src+'/', 
         dict(kind=kind, rev=int(rev), date=date, commits=int(commits), dest=dest)
     )
        for kind,rev,date,commits,src,dest in
        (line[:-1].split('\t') 
         for line in open('/Users/dave/src/ryppl/subconvert/doc/branches.txt')
         if '\t' in line))

ref_re = re.compile(
    r'\s*\[\s*(\d*)\s*:\s*(\d*)\s*\]\s*"([^"]+)"\s*:\s*"([^"]+)"\s*;\s*')

repositories_re = re.compile(
    r'(?P<head>.*?abstract repository common_branches\s*{\s*branches\s*{\s*\n)'
    r'(?P<branches>.*?)'
    r'^\s*}\s*tags\s*{\s*'
    r'(?P<tags>.*?)'
    r'(?P<tail>^\s*}.*)'
    , re.DOTALL | re.MULTILINE)

def run():        
    section = repositories_re.match(
        open("/Users/dave/src/ryppl/Boost2Git/repositories.txt").read()
    ).group

    sys.stdout.write(section('head'))
    
    refset = {'branches':None, 'tags':None}
    for kind in refset:
        refset[kind] = dict(
            (m.group(3), (m.group(1), m.group(2), m.group(4)))
            for m in ref_re.finditer(section(kind)))

    # import pprint
    # print '********'
    # pprint.pprint(refset)
    # print '********'

    for (src, info) in read_subconvert_refs().items():
        if src not in refset['branches'] and src not in refset['tags']:
            target = refset['tags' if info['kind'] == 'tag' else 'branches']
            rev = info['rev']
            target[src] = (str(rev) if rev >= 0 else '', '', info['dest'])

    separator = '''
  }
  tags
  {
'''
    for prefix,kind in (('','branches'), (separator,'tags')):
        refs = refset[kind]
        sys.stdout.write(prefix)
        for (src, (r0, r1, dst)) in sorted(
                refs.items(), lambda x,y:int(x[1][0] or 0)-int(y[1][0] or 0)):
            sys.stdout.write(
                '    [%5s:%5s] "%s" : "%s";\n'
                % (r0,r1,src,dst))
    sys.stdout.write(section('tail'))

if __name__ == '__main__':
    run()
