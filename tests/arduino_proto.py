import re, sys
src=open(sys.argv[1]).read().split("\n")
# funkcje definiowane od kolumny 0 (w tym podpisy wieloliniowe)
funcs=[]; i=0
kw=("if","for","while","switch","else","return","do","struct","enum","class","typedef","namespace","union")
while i<len(src):
    ln=src[i]
    if ln and not ln[0].isspace() and not ln.startswith(("#","//","}","/*","*")) and "(" in ln and not ln.rstrip().endswith(";"):
        first=ln.split("(")[0].split()
        if first and first[0] not in kw and not ln.lstrip().startswith(("static const","const char","struct")) or (first and first[0]=="static" and "(" in ln):
            j=i; sig=ln
            while "{" not in sig and j+1<len(src) and not sig.rstrip().endswith(";"):
                j+=1; sig+=" "+src[j].strip()
            if sig.rstrip().endswith("{") and "=" not in sig.split("(")[0]:
                proto=re.sub(r"\s*\{\s*$","",sig).strip()
                proto=re.sub(r"\s*=\s*[^,)]+(?=[,)])","",proto)      # Arduino usuwa wartosci domyslne z prototypow
                # usun wartosci domyslne? Arduino zachowuje - zostawiamy
                funcs.append((i,proto))
            i=j
    i+=1
if not funcs: print("brak funkcji"); sys.exit(1)
first=funcs[0][0]
protos=[p+";" for _,p in funcs]
out=src[:first]+["// --- prototypy (emulacja Arduino) ---"]+protos+["// --- koniec ---"]+src[first:]
open(sys.argv[2],"w").write("\n".join(out))
print("funkcji:",len(funcs),"pierwsza w linii",first+1)
