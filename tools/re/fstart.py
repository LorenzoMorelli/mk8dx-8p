import re,sys,bisect
L=open(sys.argv[1]).read().split("\n")
ins=re.compile(r"^\s*([0-9a-f]+):\s+\S+\s+(.*)$")
starts=[];prev_end=True
for l in L:
    m=ins.match(l)
    if not m: continue
    a=int(m.group(1),16); t=m.group(2).strip()
    if prev_end: starts.append(a)
    prev_end = t=="ret" or re.match(r"^(b|br)\s",t) is not None
for x in sys.argv[2:]:
    x=int(x,16); i=bisect.bisect_right(starts,x)-1
    print(hex(x),"-> func",hex(starts[i]))
