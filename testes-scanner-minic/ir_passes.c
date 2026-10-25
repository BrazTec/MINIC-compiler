/* Passes complementares -O1: CFG, DCE, CSE, LICM e reducao de forca.
 * Nenhum subprocesso ou dependencia Python. Operam na TAC renderizada.
 * Arquivo incluido pelo final de ir.c (um unico build gcc minic.c).
 */
#ifndef MINIC_IR_PASSES_INCLUDED
#define MINIC_IR_PASSES_INCLUDED

typedef struct { char *s; } TACRow;
typedef struct { TACRow *a; int n, cap; } TACRows;
typedef struct {
    char kind; /* F func, Z endfunc, L label, G goto, B ifFalse, T ifTrue,
                * R return, M move, X bin, U unary, D load, S store,
                * A alloc, C call, V call_void, P print, Q param, I read */
    char d[128], x[128], y[128], oper[32];
} TACParsed;

static void tac_add(TACRows *r, const char *s) {
    if (r->n == r->cap) { r->cap = r->cap ? r->cap * 2 : 64;
        r->a = xrealloc(r->a, (size_t)r->cap * sizeof(*r->a)); }
    r->a[r->n++].s = xstrndup_local(s, strlen(s));
}
static void tac_free(TACRows *r) {
    int i; for(i=0;i<r->n;i++) free(r->a[i].s);
    free(r->a); memset(r,0,sizeof(*r));
}
static TACRows tac_split(const char *s) {
    TACRows r={0};
    while (*s) { const char *e=strchr(s,'\n');
        size_t width=e?(size_t)(e-s):strlen(s);
        char *part=xstrndup_local(s,width);
        tac_add(&r,part); free(part);
        if (!e) break;
        s=e+1;
    }
    return r;
}
static char *tac_join(const TACRows *r) {
    StringBuffer b; int i; sb_init(&b);
    for(i=0;i<r->n;i++) { sb_append(&b,r->a[i].s); sb_append_char(&b,'\n'); }
    return sb_take(&b);
}
static void tac_replace(TACRows *r, int i, const char *str) {
    free(r->a[i].s); r->a[i].s=xstrndup_local(str,strlen(str));
}
static void tac_delete(TACRows *r, int i) {
    free(r->a[i].s);
    memmove(r->a+i,r->a+i+1,(size_t)(r->n-i-1)*sizeof(*r->a)); r->n--;
}
static void tac_insert(TACRows *r, int i, const char *str) {
    if (r->n == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 64;
        r->a = xrealloc(r->a, (size_t)r->cap * sizeof(*r->a));
    }
    memmove(r->a+i+1, r->a+i, (size_t)(r->n-i)*sizeof(*r->a));
    r->a[i].s=xstrndup_local(str,strlen(str));
    r->n++;
}
static TACParsed tac_parse(const char *line) {
    TACParsed p={0}; const char *s=line; size_t len;
    while (*s==' ') s++;
    len=strlen(s);
    if (!strncmp(s,"func ",5)) { p.kind='F'; sscanf(s+5,"%127s",p.d); return p; }
    if (!strcmp(s,"endfunc")) { p.kind='Z'; return p; }
    if (len && s[len-1]==':') { p.kind='L'; memcpy(p.d,s,len-1 <127?len-1:127); return p; }
    if (sscanf(s,"goto %127s",p.d)==1 && !strncmp(s,"goto ",5)) {p.kind='G';return p;}
    if (!strncmp(s,"ifFalse ",8) && sscanf(s,"ifFalse %127s goto %127s",p.x,p.d)==2) {p.kind='B';return p;}
    if (!strncmp(s,"ifTrue ",7) && sscanf(s,"ifTrue %127s goto %127s",p.x,p.d)==2) {p.kind='T';return p;}
    if (!strncmp(s,"return",6) && (s[6]==' '||s[6]=='\0')) {p.kind='R';sscanf(s,"return %127s",p.x);return p;}
    if (!strncmp(s,"print ",6)) {p.kind='P';sscanf(s+6,"%127s",p.x);return p;}
    if (!strncmp(s,"param ",6)) {p.kind='Q';sscanf(s+6,"%127s",p.x);return p;}
    if (!strncmp(s,"read ",5)) {p.kind='I';sscanf(s+5,"%127s",p.d);return p;}
    if (!strncmp(s,"store ",6) && sscanf(s,"store %127[^[][%127[^]]], %127s",p.x,p.y,p.d)==3) {p.kind='S';return p;}
    if (!strncmp(s,"alloc ",6) && sscanf(s,"alloc %127[^,], %127s",p.d,p.x)==2) {p.kind='A';return p;}
    if (!strncmp(s,"call ",5)) {p.kind='V';return p;}
    if (strstr(s," = call ")) {p.kind='C';sscanf(s,"%127s = call",p.d);return p;}
    if (strstr(s," = load ") && sscanf(s,"%127s = load %127[^[][%127[^]]]",p.d,p.x,p.y)==3) {p.kind='D';return p;}
    if (sscanf(s,"%127s = %127s %31s %127s",p.d,p.x,p.oper,p.y)==4) {p.kind='X';return p;}
    if (sscanf(s,"%127s = %31s %127s",p.d,p.oper,p.x)==3) {p.kind='U';return p;}
    if (sscanf(s,"%127s = %127s",p.d,p.x)==2) {p.kind='M';return p;}
    return p;
}
static bool tac_is_var(const char *s) {return (isalpha((unsigned char)s[0]) || s[0]=='_') && strcmp(s,"true") && strcmp(s,"false") && strcmp(s,"void");}
static int tac_uses(const TACParsed *ptr, const char **use) {
    #define p (*ptr)
    switch (p.kind) {
        case 'M': case 'U': case 'P': case 'Q': case 'R': case 'B': case 'T': case 'A':
            if (p.x[0] && tac_is_var(p.x)) {use[0]=p.x;return 1;} return 0;
        case 'X': case 'D': case 'S': {
            int n=0;
            if (p.kind=='S') {
                if(tac_is_var(p.x))use[n++]=p.x;
                if(tac_is_var(p.y))use[n++]=p.y;
                if(tac_is_var(p.d))use[n++]=p.d;
            } else {
                if(tac_is_var(p.x))use[n++]=p.x;
                if(tac_is_var(p.y))use[n++]=p.y;
            }
            return n;
        }
    }
    #undef p
    return 0;
}
static bool tac_def(TACParsed p) {return strchr("MUXDA CI",p.kind)!=NULL && p.kind!=' ' && p.kind!='\0';}
static bool tac_pure(TACParsed p) {return p.kind=='M'||p.kind=='U'||(p.kind=='X' && strcmp(p.oper,"/")&&strcmp(p.oper,"%"));}
static int tac_target(TACRows *r, int begin, int end, const char *name) {
    int i; for(i=begin;i<end;i++) {TACParsed p=tac_parse(r->a[i].s); if(p.kind=='L'&&!strcmp(p.d,name))return i;}return -1;
}
static int tac_succ(TACRows *r,int i,int beg,int end,int out[2]) {
    TACParsed p=tac_parse(r->a[i].s); int n=0, target;
    if(p.kind=='R'||p.kind=='Z')return 0;
    if(p.kind=='G'||p.kind=='B'||p.kind=='T') {
        target=tac_target(r,beg,end,p.d);
        if(target>=0)out[n++]=target;
        if(p.kind=='G')return n;
    }
    if(i+1<end)out[n++]=i+1;
    return n;
}
/* Remove codigo inalcançavel em cada função sem remover os marcadores endfunc. */
static void tac_unreachable(TACRows *r) {
    int i=0;
    while (i<r->n) {
        int j=i+1, k, changed=1; unsigned char *seen;
        if(tac_parse(r->a[i].s).kind!='F'){i++;continue;}
        while(j<r->n && tac_parse(r->a[j].s).kind!='Z')j++;
        if(j>=r->n)break;
        seen=xcalloc((size_t)(j-i+1),1);seen[0]=1;
        while(changed){changed=0;
            for(k=i;k<j;k++)if(seen[k-i]){int edges[2], e, n=tac_succ(r,k,i,j+1,edges);
                for(e=0;e<n;e++)if(!seen[edges[e]-i]){seen[edges[e]-i]=1;changed=1;}}
        }
        for(k=j-1;k>i;k--)if(!seen[k-i]){tac_delete(r,k);j--;}
        free(seen);i=j+1;
    }
}
static void tac_branches(TACRows *r) {
    int i;
    for(i=0;i<r->n;i++){
        TACParsed p=tac_parse(r->a[i].s);
        if(p.kind=='B'||p.kind=='T'){
            bool is_true=!strcmp(p.x,"true")||!strcmp(p.x,"1");
            bool is_false=!strcmp(p.x,"false")||!strcmp(p.x,"0");
            if(is_true||is_false){
                bool branch=p.kind=='T'?is_true:is_false;
                if(branch){char line[256];snprintf(line,sizeof(line),"  goto %s",p.d);tac_replace(r,i,line);}
                else {tac_delete(r,i);i--;}
            }
        }
    }
}
static void tac_labels(TACRows *r){
    int repeat,i,j;
    for(repeat=0;repeat<4;repeat++){
        int change=0;
        for(i=r->n-1;i>=0;i--){
            TACParsed p=tac_parse(r->a[i].s);
            if(p.kind=='L'){
                int used=0;
                for(j=0;j<r->n;j++){TACParsed q=tac_parse(r->a[j].s);
                    if((q.kind=='G'||q.kind=='B'||q.kind=='T')&&!strcmp(q.d,p.d)){used=1;break;}}
                if(!used){tac_delete(r,i);change++;}
            }else if(p.kind=='G'&&i+1<r->n){
                TACParsed q=tac_parse(r->a[i+1].s);
                if(q.kind=='L'&&!strcmp(q.d,p.d)){tac_delete(r,i);change++;}
            }
        }
        if(!change)break;
    }
}
/* LICM: preheader estruturado; só operações não excepcionais, sem redefs. */
static void tac_licm(TACRows *r) {
    int i;
    for(i=0;i<r->n;i++){
        TACParsed head=tac_parse(r->a[i].s);int j,k,unsafe=0, has_branch=0, chosen=-1;
        if(head.kind!='L'||!strstr(head.d,"_while"))continue;
        j=i+1;
        while(j<r->n){TACParsed p=tac_parse(r->a[j].s);
            if(p.kind=='G'&&!strcmp(p.d,head.d))break;
            if(p.kind=='F'||p.kind=='Z')break;
            j++;
        }
        if(j>=r->n)continue;
        for(k=i;k<=j;k++){
            TACParsed p=tac_parse(r->a[k].s);
            if(p.kind=='B')has_branch=1;
            if(p.kind=='C'||p.kind=='V'||p.kind=='I'||p.kind=='S')unsafe=1;
            if(k>i && p.kind=='L' && strstr(p.d,"_while"))unsafe=1;
        }
        if(unsafe||!has_branch)continue;
        for(k=i+1;k<j;k++){
            TACParsed p=tac_parse(r->a[k].s);int m,variant=0;
            if(p.kind!='X'||!strchr("+-*",p.oper[0])||p.oper[1]!='\0'||p.d[0]!='t'||!isdigit((unsigned char)p.d[1]))continue;
            for(m=i;m<=j;m++){
                TACParsed q=tac_parse(r->a[m].s);
                if(tac_def(q) && (!strcmp(q.d,p.x)||!strcmp(q.d,p.y)))variant=1;
            }
            if(!variant){chosen=k;break;}
        }
        if(chosen>=0){char *mov=xstrndup_local(r->a[chosen].s,strlen(r->a[chosen].s));
            tac_delete(r,chosen);tac_insert(r,i,mov);free(mov);i++;}
    }
}
typedef struct {char op[32],x[128],y[128],d[128];} TACExpr;
static void tac_cse(TACRows *r) {
    TACExpr *expressions=xcalloc((size_t)(r->n+1),sizeof(*expressions));
    int i,count=0;
    for(i=0;i<r->n;i++){
        TACParsed p=tac_parse(r->a[i].s);int k;
        if(tac_def(p))for(k=count-1;k>=0;k--){
            if(!strcmp(p.d,expressions[k].x)||!strcmp(p.d,expressions[k].y)||!strcmp(p.d,expressions[k].d))
                {memmove(expressions+k,expressions+k+1,(size_t)(count-k-1)*sizeof(*expressions));count--;}}
        if(p.kind=='X'&&strcmp(p.oper,"/")&&strcmp(p.oper,"%")){
            char *x=p.x,*y=p.y;
            int comm=(!strcmp(p.oper,"+")||!strcmp(p.oper,"*")||!strcmp(p.oper,"==")||!strcmp(p.oper,"!="));
            if(comm&&strcmp(x,y)>0){x=p.y;y=p.x;}
            for(k=0;k<count;k++)if(!strcmp(expressions[k].op,p.oper)&&!strcmp(expressions[k].x,x)&&!strcmp(expressions[k].y,y))break;
            if(k<count){char line[320];snprintf(line,sizeof(line),"  %s = %s",p.d,expressions[k].d);tac_replace(r,i,line);}
            else {TACExpr *e=expressions+count++;strcpy(e->op,p.oper);strcpy(e->x,x);strcpy(e->y,y);strcpy(e->d,p.d);}
        }
        if(strchr("LGBT FZCVSIR",p.kind)!=NULL&&p.kind!='\0')count=0;
    }
    free(expressions);
}
static void tac_strength(TACRows *r){int i;
    for(i=0;i<r->n;i++){TACParsed p=tac_parse(r->a[i].s);
        if(p.kind=='X'&&!strcmp(p.oper,"*")&&(!strcmp(p.x,"2")||!strcmp(p.y,"2"))){
            const char *var=!strcmp(p.x,"2")?p.y:p.x;
            if(tac_is_var(var)){char line[512];snprintf(line,sizeof(line),"  %s = %s + %s",p.d,var,var);tac_replace(r,i,line);}
        }
    }}
/* DCE: análise de vivacidade intraprocedural em CFG, conservadora quanto a
 * variáveis compartilhadas entre funções; nunca elimina call/load/store/read.
 */
static void tac_dead(TACRows *r){
    int i, changed=1;
    while(changed){
        int fn_start=-1, fn_end=-1;
        changed=0;
        for(i=0;i<r->n;i++){
            TACParsed p=tac_parse(r->a[i].s);
            if(p.kind=='F')fn_start=i;
            if(p.kind=='Z' && fn_start>=0){
                int n=i-fn_start+1,k,sweep,iter;
                char **names=xcalloc((size_t)(n*4+16),sizeof(*names));int nname=0;
                unsigned char *in,*out;
                fn_end=i;
                for(k=fn_start;k<=fn_end;k++){
                    const char *use[4];int u,v,num;
                    TACParsed q=tac_parse(r->a[k].s);
                    const char *parts[5];int count=0;
                    if(tac_def(q))parts[count++]=q.d;
                    num=tac_uses(&q,use);
                    for(u=0;u<num;u++)parts[count++]=use[u];
                    for(u=0;u<count;u++){
                        if(!tac_is_var(parts[u]))continue;
                        for(v=0;v<nname;v++)if(!strcmp(names[v],parts[u]))break;
                        if(v==nname)names[nname++]=xstrndup_local(parts[u],strlen(parts[u]));
                    }
                }
                in=xcalloc((size_t)n*(size_t)(nname+1),1);
                out=xcalloc((size_t)n*(size_t)(nname+1),1);
                for(iter=0;iter<n*3+10;iter++){
                    int changed_live=0;
                    for(k=fn_end;k>=fn_start;k--){
                        int edges[2],m,num=tac_succ(r,k,fn_start,fn_end+1,edges),v;
                        TACParsed q=tac_parse(r->a[k].s);const char *use[4];int nu=tac_uses(&q,use);
                        unsigned char *o=out+(size_t)(k-fn_start)*(nname+1);
                        unsigned char *li=in+(size_t)(k-fn_start)*(nname+1);
                        for(v=0;v<nname;v++){
                            unsigned char tmp=0,val;
                            for(m=0;m<num;m++)tmp |= in[(size_t)(edges[m]-fn_start)*(nname+1)+v];
                            if(o[v]!=tmp){o[v]=tmp;changed_live=1;}
                            val=tmp;
                            if(tac_def(q)&&!strcmp(q.d,names[v]))val=0;
                            for(m=0;m<nu;m++)if(!strcmp(use[m],names[v]))val=1;
                            if(li[v]!=val){li[v]=val;changed_live=1;}
                        }
                    }
                    if(!changed_live)break;
                }
                for(k=fn_end-1;k>fn_start;k--){
                    TACParsed q=tac_parse(r->a[k].s);int index,used=0;
                    if(!tac_pure(q)||!tac_def(q))continue;
                    for(index=0;index<nname;index++)if(!strcmp(names[index],q.d))break;
                    if(index==nname)continue;
                    if(out[(size_t)(k-fn_start)*(nname+1)+index])continue;
                    /* Se o nome aparece em outra função, preservar seu estado. */
                    for(sweep=0;sweep<r->n;sweep++)if(sweep<fn_start||sweep>fn_end){
                        TACParsed z=tac_parse(r->a[sweep].s); const char *usednames[4];int u,nu=tac_uses(&z,usednames);
                        if(tac_def(z)&&!strcmp(z.d,q.d))used=1;
                        for(u=0;u<nu;u++)if(!strcmp(usednames[u],q.d))used=1;
                        if(used)break;
                    }
                    if(!used){tac_delete(r,k);changed=1;}
                }
                for(k=0;k<nname;k++)free(names[k]);
                free(names);free(in);free(out);
                if(changed)break;
                fn_start=-1;
            }
        }
    }
}
static char *ir_optimize_advanced(const char *raw) {
    TACRows r=tac_split(raw); int iteration;
    char *stage;
    tac_licm(&r);
    tac_cse(&r);
    stage=tac_join(&r);tac_free(&r);
    for(iteration=0;iteration<4;iteration++){
        char *folded=ir_optimize_text(stage);free(stage);
        r=tac_split(folded);free(folded);
        tac_strength(&r);
        tac_branches(&r);tac_unreachable(&r);tac_labels(&r);tac_dead(&r);
        stage=tac_join(&r);tac_free(&r);
    }
    return stage;
}
#endif
