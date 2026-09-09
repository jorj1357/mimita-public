// 09 01 2026, 00 00
/* p[ServerGamemode][ServerGamemode]pos[ServerGamemode]
* Imp[ServerGamemode][ServerGamemode]m[ServerGamemode]nts th[ServerGamemode] a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] P[ServerGamemode]P d[ServerGamemode][ServerGamemode][ServerGamemode] stat[ServerGamemode] machin[ServerGamemode] on th[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].
* Waits fo[ServerGamemode] two p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, [ServerGamemode][ServerGamemode]ns a sing[ServerGamemode][ServerGamemode] co[ServerGamemode]ntdown, sco[ServerGamemode][ServerGamemode]s fi[ServerGamemode]st-to-goa[ServerGamemode],
* instant-[ServerGamemode][ServerGamemode]spawns [ServerGamemode]ictims at th[ServerGamemode]i[ServerGamemode] t[ServerGamemode]am spawn, and a[ServerGamemode]to-[ServerGamemode][ServerGamemode]match[ServerGamemode]s aft[ServerGamemode][ServerGamemode] th[ServerGamemode]
* post-match window whi[ServerGamemode][ServerGamemode] both p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s a[ServerGamemode][ServerGamemode] sti[ServerGamemode][ServerGamemode] conn[ServerGamemode]ct[ServerGamemode]d.
* A[ServerGamemode]so s[ServerGamemode]ppo[ServerGamemode]ts FFA and T[ServerGamemode]M match mod[ServerGamemode]s with m[ServerGamemode][ServerGamemode]ti-p[ServerGamemode]ay[ServerGamemode][ServerGamemode] sco[ServerGamemode]ing.
* [ServerGamemode]o[ServerGamemode]s NOT app[ServerGamemode]y damag[ServerGamemode], sim[ServerGamemode][ServerGamemode]at[ServerGamemode] mo[ServerGamemode][ServerGamemode]m[ServerGamemode]nt, o[ServerGamemode] [ServerGamemode][ServerGamemode]nd[ServerGamemode][ServerGamemode] anything.
* [ServerGamemode]o[ServerGamemode]s NOT to[ServerGamemode]ch th[ServerGamemode] c[ServerGamemode]i[ServerGamemode]nt q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]/matchmaking o[ServerGamemode] coo[ServerGamemode]dinato[ServerGamemode] p[ServerGamemode]otoco[ServerGamemode].
* [ServerGamemode]o[ServerGamemode]s NOT a[ServerGamemode]t[ServerGamemode][ServerGamemode] no[ServerGamemode]ma[ServerGamemode] (non-d[ServerGamemode][ServerGamemode][ServerGamemode]) s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] b[ServerGamemode]ha[ServerGamemode]io[ServerGamemode].
*/

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "n[ServerGamemode]two[ServerGamemode]k/s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]-gam[ServerGamemode]mod[ServerGamemode].h"

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <a[ServerGamemode]go[ServerGamemode]ithm>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <cmath>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <[ServerGamemode]andom>

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "n[ServerGamemode]two[ServerGamemode]k/pack[ServerGamemode]ts.h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "n[ServerGamemode]two[ServerGamemode]k/s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "npc/npc.h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "combat/w[ServerGamemode]apon-[ServerGamemode][ServerGamemode]gist[ServerGamemode]y.h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "d[ServerGamemode]b[ServerGamemode]g/d[ServerGamemode]b[ServerGamemode]g-[ServerGamemode]og.h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "n[ServerGamemode]two[ServerGamemode]k/comm[ServerGamemode]nity-s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]-config.h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "gam[ServerGamemode]mod[ServerGamemode]/gam[ServerGamemode]mod[ServerGamemode].h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "p[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]/p[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]-q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].h"
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "p[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]/p[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]-[ServerGamemode][ServerGamemode][ServerGamemode]nts.h"

nam[ServerGamemode]spac[ServerGamemode] MimitaN[ServerGamemode]t {

[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]()
{
    static [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode] stat[ServerGamemode];
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n stat[ServerGamemode];
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ta[ServerGamemode]tMod[ServerGamemode](const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.mapOn[ServerGamemode]y = fa[ServerGamemode]s[ServerGamemode];
    d.mod[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.matchMod[ServerGamemode] == "tdm" ? [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::T[ServerGamemode]am[ServerGamemode][ServerGamemode]athmatch
        : [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.matchMod[ServerGamemode] == "ffa" ? [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::F[ServerGamemode][ServerGamemode][ServerGamemode]Fo[ServerGamemode]A[ServerGamemode][ServerGamemode]
        : [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.matchMod[ServerGamemode] == "d[ServerGamemode][ServerGamemode][ServerGamemode]" ? [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] : [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::[ServerGamemode]andbox;
    d.comm[ServerGamemode]nityMod[ServerGamemode] = "sandbox";
    d.comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId = 1;
    d.comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tExp[ServerGamemode]icit = fa[ServerGamemode]s[ServerGamemode];
    d.comm[ServerGamemode]nity[ServerGamemode]co[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.comm[ServerGamemode]nityT[ServerGamemode]ams.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][0] = d.comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][1] = 0;
    d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs = 0;
    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING;
    d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = fa[ServerGamemode]s[ServerGamemode];
    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.sco[ServerGamemode][ServerGamemode]A = 0;
    d.sco[ServerGamemode][ServerGamemode]B = 0;
    d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode];
    d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds;
    d.[ServerGamemode][ServerGamemode]match[ServerGamemode][ServerGamemode]conds = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.[ServerGamemode][ServerGamemode]match[ServerGamemode][ServerGamemode]conds;
    d.t[ServerGamemode]amANam[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.t[ServerGamemode]amANam[ServerGamemode];
    d.t[ServerGamemode]amBNam[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.t[ServerGamemode]amBNam[ServerGamemode];
    d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId = 0;
    d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId = 0;
    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    d.spawnsAssign[ServerGamemode]d = fa[ServerGamemode]s[ServerGamemode];
    d.stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
    d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s;
    d.mapPoo[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapPoo[ServerGamemode];
    d.[ServerGamemode]otat[ServerGamemode]Maps = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.[ServerGamemode]otat[ServerGamemode]Maps;
    d.mapId = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapId;
    d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id = 0;
    d.mapV[ServerGamemode][ServerGamemode]sion = 0;
    d.spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = 0;
    d.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode] = 0;
    d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = 0;
    d.spawnAncho[ServerGamemode]Ind[ServerGamemode]x = 0;
    d.[ServerGamemode]s[ServerGamemode]dMaps.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.[ServerGamemode]s[ServerGamemode]dMaps.ins[ServerGamemode][ServerGamemode]t([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapId);
    d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = fa[ServerGamemode]s[ServerGamemode];
    d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.a[ServerGamemode]toMapRotation = fa[ServerGamemode]s[ServerGamemode];
    d.mapRotationMin[ServerGamemode]t[ServerGamemode]s = 15;
    d.n[ServerGamemode]xtMapRotationMs = 0;
    d.mapChang[ServerGamemode]Co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tMs = 0;
    d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();

    // ── FFA/T[ServerGamemode]M fi[ServerGamemode][ServerGamemode]ds ─────────────────────────────────────────────
    d.matchMod[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.matchMod[ServerGamemode];
    d.co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tTick = 0;
    d.match[ServerGamemode]ta[ServerGamemode]tTick = 0;
    d.matchTim[ServerGamemode]LimitTick = 0;
    d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = 0.0f;
    d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
    d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds;
    d.ffaKi[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.ffa[ServerGamemode][ServerGamemode]aths.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.matchT[ServerGamemode]ams.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.pa[ServerGamemode]ticipants.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 0;
    d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = -1;
    d.ki[ServerGamemode][ServerGamemode]E[ServerGamemode][ServerGamemode]ntCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;

    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] [ServerGamemode]nab[ServerGamemode][ServerGamemode]d mod[ServerGamemode]=%s goa[ServerGamemode]=%d co[ServerGamemode]ntdown=%.1fs [ServerGamemode][ServerGamemode]match=%.1fs t[ServerGamemode]ams=%s/%s [ServerGamemode]otat[ServerGamemode]=%d poo[ServerGamemode]=%z[ServerGamemode] offs[ServerGamemode]t=%.1f tim[ServerGamemode]Limit=%d int[ServerGamemode][ServerGamemode]mission=%d [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts=%d\n",
        d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode], d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds, d.[ServerGamemode][ServerGamemode]match[ServerGamemode][ServerGamemode]conds,
        d.t[ServerGamemode]amANam[ServerGamemode].c_st[ServerGamemode](), d.t[ServerGamemode]amBNam[ServerGamemode].c_st[ServerGamemode](), (int)d.[ServerGamemode]otat[ServerGamemode]Maps, d.mapPoo[ServerGamemode].siz[ServerGamemode](),
        d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s, d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds, (int)d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds, (int)d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds);
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]ta[ServerGamemode]t(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s)
{
    s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ta[ServerGamemode]tMod[ServerGamemode]([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s);
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityMap[ServerGamemode]ta[ServerGamemode]t(const std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing>& mapPoo[ServerGamemode],
                             const std::st[ServerGamemode]ing& mapId,
                             boo[ServerGamemode] a[ServerGamemode]toRotation,
                             [ServerGamemode]int32_t [ServerGamemode]otationMin[ServerGamemode]t[ServerGamemode]s,
                             int w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s;
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapPoo[ServerGamemode] = mapPoo[ServerGamemode];
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapId = mapId;
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.[ServerGamemode]otat[ServerGamemode]Maps = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapOn[ServerGamemode]y = t[ServerGamemode][ServerGamemode][ServerGamemode];
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.a[ServerGamemode]toMapRotation = a[ServerGamemode]toRotation;
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapRotationMin[ServerGamemode]t[ServerGamemode]s = std::c[ServerGamemode]amp([ServerGamemode]otationMin[ServerGamemode]t[ServerGamemode]s, 1[ServerGamemode], 9999[ServerGamemode]);
    s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ta[ServerGamemode]tMod[ServerGamemode]([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s);
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& stat[ServerGamemode] = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    stat[ServerGamemode].mapOn[ServerGamemode]y = t[ServerGamemode][ServerGamemode][ServerGamemode];
    stat[ServerGamemode].mod[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::[ServerGamemode]andbox;
    stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode] = "sandbox";
    stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId = std::max(1, w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId);
    stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tExp[ServerGamemode]icit = t[ServerGamemode][ServerGamemode][ServerGamemode];
    stat[ServerGamemode].a[ServerGamemode]toMapRotation = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.a[ServerGamemode]toMapRotation;
    stat[ServerGamemode].mapRotationMin[ServerGamemode]t[ServerGamemode]s = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.mapRotationMin[ServerGamemode]t[ServerGamemode]s;
    stat[ServerGamemode].n[ServerGamemode]xtMapRotationMs = nowMs() + ([ServerGamemode]int64_t)stat[ServerGamemode].mapRotationMin[ServerGamemode]t[ServerGamemode]s * 60000[ServerGamemode][ServerGamemode][ServerGamemode];
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
        "[COMMUNITY MAP RUNTIME] map=%s a[ServerGamemode]to=%d int[ServerGamemode][ServerGamemode][ServerGamemode]a[ServerGamemode]Min[ServerGamemode]t[ServerGamemode]s=%[ServerGamemode] poo[ServerGamemode]=%z[ServerGamemode]\n",
        mapId.c_st[ServerGamemode](), (int)a[ServerGamemode]toRotation, stat[ServerGamemode].mapRotationMin[ServerGamemode]t[ServerGamemode]s, mapPoo[ServerGamemode].siz[ServerGamemode]());
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode]tW[ServerGamemode]apon[ServerGamemode][ServerGamemode]t(int w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& stat[ServerGamemode] = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!stat[ServerGamemode].[ServerGamemode]nab[ServerGamemode][ServerGamemode]d || !stat[ServerGamemode].mapOn[ServerGamemode]y) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]ts().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    if (!config.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tById(w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId)) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId = w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId;
    stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tExp[ServerGamemode]icit = t[ServerGamemode][ServerGamemode][ServerGamemode];
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
        "[COMMUNITY WEAPON [ServerGamemode]ET] s[ServerGamemode][ServerGamemode][ServerGamemode]ct[ServerGamemode]d=%d\n", stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId);
}

boo[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponA[ServerGamemode][ServerGamemode]ow[ServerGamemode]d(const std::st[ServerGamemode]ing& w[ServerGamemode]aponId)
{
    const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& stat[ServerGamemode] = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    const boo[ServerGamemode] comm[ServerGamemode]nityMod[ServerGamemode] = stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode] == "sandbox"
        || stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode] == "f[ServerGamemode][ServerGamemode][ServerGamemode]_fo[ServerGamemode]_a[ServerGamemode][ServerGamemode]"
        || stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode] == "t[ServerGamemode]am_d[ServerGamemode]athmatch"
        || stat[ServerGamemode].hasBombF[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode];
    if (!comm[ServerGamemode]nityMod[ServerGamemode]) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n t[ServerGamemode][ServerGamemode][ServerGamemode];
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]ts().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n config.w[ServerGamemode]aponA[ServerGamemode][ServerGamemode]ow[ServerGamemode]d(stat[ServerGamemode].comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId, w[ServerGamemode]aponId);
}

int s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponNati[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ot(int [ServerGamemode]ogica[ServerGamemode][ServerGamemode][ServerGamemode]ot)
{
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]ts().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    const std::st[ServerGamemode]ing* id = config.w[ServerGamemode]aponFo[ServerGamemode][ServerGamemode][ServerGamemode]ot(s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]().comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId, [ServerGamemode]ogica[ServerGamemode][ServerGamemode][ServerGamemode]ot);
    if (!id) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n [ServerGamemode]ogica[ServerGamemode][ServerGamemode][ServerGamemode]ot;
    const W[ServerGamemode]apon[ServerGamemode][ServerGamemode]finition* d[ServerGamemode]f = W[ServerGamemode]aponR[ServerGamemode]gist[ServerGamemode]y::instanc[ServerGamemode]().g[ServerGamemode]t(*id);
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n d[ServerGamemode]f ? d[ServerGamemode]f->s[ServerGamemode]ot : -1;
}

int s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponLogica[ServerGamemode][ServerGamemode][ServerGamemode]ot(const std::st[ServerGamemode]ing& w[ServerGamemode]aponId)
{
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]ts().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    const int s[ServerGamemode]ot = config.s[ServerGamemode]otFo[ServerGamemode]W[ServerGamemode]apon(s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]().comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId, w[ServerGamemode]aponId);
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n s[ServerGamemode]ot > 0 ? s[ServerGamemode]ot : -1;
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode]tMod[ServerGamemode](const std::st[ServerGamemode]ing& mod[ServerGamemode]Id)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& stat[ServerGamemode] = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!stat[ServerGamemode].[ServerGamemode]nab[ServerGamemode][ServerGamemode]d || mod[ServerGamemode]Id.[ServerGamemode]mpty()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.mod[ServerGamemode]s().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode] = mod[ServerGamemode]Id;
    stat[ServerGamemode].comm[ServerGamemode]nity[ServerGamemode]co[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    stat[ServerGamemode].comm[ServerGamemode]nityT[ServerGamemode]ams.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    stat[ServerGamemode].comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][0] = stat[ServerGamemode].comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][1] = 0;
    stat[ServerGamemode].comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    stat[ServerGamemode].comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs = 0;
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
        "[COMMUNITY MO[ServerGamemode]E] s[ServerGamemode][ServerGamemode][ServerGamemode]ct[ServerGamemode]d=%s\n", stat[ServerGamemode].comm[ServerGamemode]nityMod[ServerGamemode].c_st[ServerGamemode]());
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode]ta[ServerGamemode]tMatch(boo[ServerGamemode] skipInt[ServerGamemode][ServerGamemode]mission, const std::st[ServerGamemode]ing& [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dMod[ServerGamemode])
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;

    if (![ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dMod[ServerGamemode].[ServerGamemode]mpty())
        d.comm[ServerGamemode]nityMod[ServerGamemode] = [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dMod[ServerGamemode];

    // ── Look [ServerGamemode]p th[ServerGamemode] comm[ServerGamemode]nity mod[ServerGamemode] and [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode] its gam[ServerGamemode]mod[ServerGamemode]_id ───────
    const Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& comm[ServerGamemode]nityConfig = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    const Comm[ServerGamemode]nityMod[ServerGamemode]* cm = comm[ServerGamemode]nityConfig.mod[ServerGamemode]ById(d.comm[ServerGamemode]nityMod[ServerGamemode]);
    if (!cm) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;  // [ServerGamemode]nknown mod[ServerGamemode] — cannot sta[ServerGamemode]t

    // Us[ServerGamemode] gam[ServerGamemode]mod[ServerGamemode]_id to [ServerGamemode]ook [ServerGamemode]p th[ServerGamemode] act[ServerGamemode]a[ServerGamemode] gam[ServerGamemode]mod[ServerGamemode] config.
    // This b[ServerGamemode]idg[ServerGamemode]s on[ServerGamemode]in[ServerGamemode]mod[ServerGamemode]s.json (comm[ServerGamemode]nity m[ServerGamemode]n[ServerGamemode]) to gam[ServerGamemode]mod[ServerGamemode]s/*.json (gam[ServerGamemode]p[ServerGamemode]ay [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s).
    const std::st[ServerGamemode]ing& [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id = cm->gam[ServerGamemode]mod[ServerGamemode]Id;

    // [ServerGamemode][ServerGamemode]f[ServerGamemode][ServerGamemode] a [ServerGamemode]i[ServerGamemode][ServerGamemode] mod[ServerGamemode] switch [ServerGamemode]nti[ServerGamemode] th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt mod[ServerGamemode] has shown its [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts.
    if (!d.mapOn[ServerGamemode]y && !d.matchMod[ServerGamemode].[ServerGamemode]mpty() && d.matchMod[ServerGamemode] != [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id &&
        d.phas[ServerGamemode] != [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING && d.phas[ServerGamemode] != [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode])
    {
        d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witch = t[ServerGamemode][ServerGamemode][ServerGamemode];
        d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witchCo[ServerGamemode]ntdown = skipInt[ServerGamemode][ServerGamemode]mission;
        d.p[ServerGamemode]ndingGam[ServerGamemode]mod[ServerGamemode]Id = [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id;
        d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode];
        d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = 5.0f;
        d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
        if (d.matchMod[ServerGamemode] == "ffa") {
            int b[ServerGamemode]st = -1;
            fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : d.ffaKi[ServerGamemode][ServerGamemode]s)
                if (k[ServerGamemode].s[ServerGamemode]cond > b[ServerGamemode]st) { b[ServerGamemode]st = k[ServerGamemode].s[ServerGamemode]cond; d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = k[ServerGamemode].fi[ServerGamemode]st; }
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "tdm") {
            d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s >= d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s ? 0 : 1;
        }
        d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = t[ServerGamemode][ServerGamemode][ServerGamemode];
        ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[GAMEMO[ServerGamemode]E [ServerGamemode]WITCH] o[ServerGamemode]d=%s n[ServerGamemode]w=%s [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds=5 co[ServerGamemode]ntdownAft[ServerGamemode][ServerGamemode]=%d\n",
            d.matchMod[ServerGamemode].c_st[ServerGamemode](), [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id.c_st[ServerGamemode](), (int)skipInt[ServerGamemode][ServerGamemode]mission);
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    }

    // [ServerGamemode][ServerGamemode]t match mod[ServerGamemode] f[ServerGamemode]om comm[ServerGamemode]nity mod[ServerGamemode] id ([ServerGamemode]s[ServerGamemode]d fo[ServerGamemode] [ServerGamemode]o[ServerGamemode]ting and stat[ServerGamemode] machin[ServerGamemode])
    d.matchMod[ServerGamemode] = [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id;
    // [ServerGamemode]EPRECATE[ServerGamemode]: th[ServerGamemode] o[ServerGamemode]d [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode] [ServerGamemode]n[ServerGamemode]m and sho[ServerGamemode]t-fo[ServerGamemode]m matchMod[ServerGamemode] st[ServerGamemode]ings
    // a[ServerGamemode][ServerGamemode] k[ServerGamemode]pt fo[ServerGamemode] backwa[ServerGamemode]d compatibi[ServerGamemode]ity with d[ServerGamemode][ServerGamemode][ServerGamemode]/FFA/T[ServerGamemode]M cod[ServerGamemode] paths.
    // N[ServerGamemode]w mod[ServerGamemode]s sho[ServerGamemode][ServerGamemode]d [ServerGamemode]s[ServerGamemode] matchMod[ServerGamemode] di[ServerGamemode][ServerGamemode]ct[ServerGamemode]y (th[ServerGamemode] comm[ServerGamemode]nity mod[ServerGamemode] id).
    d.mod[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::[ServerGamemode]andbox;

    // Load gam[ServerGamemode]mod[ServerGamemode] config [ServerGamemode]sing th[ServerGamemode] [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]d gam[ServerGamemode]mod[ServerGamemode]_id
    const Gam[ServerGamemode]mod[ServerGamemode]& gm = Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]gist[ServerGamemode]y::instanc[ServerGamemode]().g[ServerGamemode]t([ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id);
    d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] = gm.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode];
    // An [ServerGamemode]xp[ServerGamemode]icit GUI/[ServerGamemode][ServerGamemode]ntim[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode]ction wins o[ServerGamemode][ServerGamemode][ServerGamemode] th[ServerGamemode] gam[ServerGamemode]mod[ServerGamemode] d[ServerGamemode]fa[ServerGamemode][ServerGamemode]t.
    if (!d.comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tExp[ServerGamemode]icit && gm.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId > 0)
        d.comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId = gm.w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId;
    d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds = gm.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds;
    d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds = (f[ServerGamemode]oat)gm.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds = (f[ServerGamemode]oat)gm.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
    d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds = gm.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds;
    d.go[ServerGamemode][ServerGamemode]conds = gm.go[ServerGamemode][ServerGamemode]conds;
    d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s = gm.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s;
    d.hasBombF[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode] = gm.f[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode]s.bombHo[ServerGamemode]d[ServerGamemode][ServerGamemode]T[ServerGamemode]xt;

    // mod[ServerGamemode]sta[ServerGamemode]t [ServerGamemode]nt[ServerGamemode][ServerGamemode]s th[ServerGamemode] config[ServerGamemode][ServerGamemode][ServerGamemode]d int[ServerGamemode][ServerGamemode]mission. mod[ServerGamemode]sta[ServerGamemode]tnow [ServerGamemode]nt[ServerGamemode][ServerGamemode]s th[ServerGamemode]
    // co[ServerGamemode]ntdown di[ServerGamemode][ServerGamemode]ct[ServerGamemode]y; s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]Tick owns th[ServerGamemode] a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] 3-2-1.
    d.mapOn[ServerGamemode]y = fa[ServerGamemode]s[ServerGamemode];
    d.[ServerGamemode]astB[ServerGamemode]oadcastTick = 0;
    d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.[ServerGamemode]otat[ServerGamemode]Maps = d.a[ServerGamemode]toMapRotation;
    d.ffaKi[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.ffa[ServerGamemode][ServerGamemode]aths.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.matchT[ServerGamemode]ams.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.pa[ServerGamemode]ticipants.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.spawnsAssign[ServerGamemode]d = fa[ServerGamemode]s[ServerGamemode];
    d.sta[ServerGamemode]tCo[ServerGamemode]ntdownImm[ServerGamemode]diat[ServerGamemode][ServerGamemode]y = skipInt[ServerGamemode][ServerGamemode]mission;
    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION;
    d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = skipInt[ServerGamemode][ServerGamemode]mission ? 0.0f : d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = -1;
    ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    ++d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;

    // ── Mod[ServerGamemode]-sp[ServerGamemode]cific acti[ServerGamemode]ation ─────────────────────────────────────
    // Each mod[ServerGamemode] that n[ServerGamemode][ServerGamemode]ds [ServerGamemode]xt[ServerGamemode]a initia[ServerGamemode]ization g[ServerGamemode]ts its [ServerGamemode]nt[ServerGamemode]y point ca[ServerGamemode][ServerGamemode][ServerGamemode]d h[ServerGamemode][ServerGamemode][ServerGamemode].
    // This [ServerGamemode][ServerGamemode]p[ServerGamemode]ac[ServerGamemode]s th[ServerGamemode] o[ServerGamemode]d ha[ServerGamemode]dcod[ServerGamemode]d if/[ServerGamemode][ServerGamemode]s[ServerGamemode] if chain.
    if (d.hasBombF[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode]) {
        s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTag[ServerGamemode]ta[ServerGamemode]tMatch(skipInt[ServerGamemode][ServerGamemode]mission);
    }

    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[MO[ServerGamemode]E[ServerGamemode]TART] comm[ServerGamemode]nity=%s gam[ServerGamemode]mod[ServerGamemode]=%s phas[ServerGamemode]=%s goa[ServerGamemode]=%d tim[ServerGamemode]Limit=%d int[ServerGamemode][ServerGamemode]mission=%.0f\n",
        d.comm[ServerGamemode]nityMod[ServerGamemode].c_st[ServerGamemode](), [ServerGamemode][ServerGamemode]so[ServerGamemode][ServerGamemode][ServerGamemode]dGam[ServerGamemode]mod[ServerGamemode]Id.c_st[ServerGamemode](),
        skipInt[ServerGamemode][ServerGamemode]mission ? "COUNT[ServerGamemode]OWN_PEN[ServerGamemode]ING" : "INTERMI[ServerGamemode][ServerGamemode]ION",
        d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode],
        d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds, d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds);
}

nam[ServerGamemode]spac[ServerGamemode] {

[ServerGamemode]int32_t co[ServerGamemode]ntActi[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]s(const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    [ServerGamemode]int32_t co[ServerGamemode]nt = 0;
    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] == [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            ++co[ServerGamemode]nt;
    }
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n co[ServerGamemode]nt;
}

[ServerGamemode]oid b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode]([ServerGamemode]OCKET sock,
                        const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                        const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                        [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode]Pack[ServerGamemode]t pkt{};
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_[ServerGamemode]UEL_[ServerGamemode]TATE;
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].tick = 0;
    pkt.phas[ServerGamemode] = d.phas[ServerGamemode];
    pkt.d[ServerGamemode][ServerGamemode][ServerGamemode]Id = d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
    pkt.mapV[ServerGamemode][ServerGamemode]sion = d.mapV[ServerGamemode][ServerGamemode]sion;
    pkt.spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = d.spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    pkt.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode] = d.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode];
    pkt.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    std::st[ServerGamemode]ncpy(pkt.mapId, d.mapId.c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.mapId) - 1);
    pkt.spawnAncho[ServerGamemode]Ind[ServerGamemode]x = d.spawnAncho[ServerGamemode]Ind[ServerGamemode]x;
    pkt.ancho[ServerGamemode]X = d.spawnA.x;
    pkt.ancho[ServerGamemode]Y = d.spawnA.y;
    pkt.ancho[ServerGamemode]Z = d.spawnA.z;
    a[ServerGamemode]to a = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId);
    a[ServerGamemode]to b = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId);
    if (a != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) {
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]A[ServerGamemode]pawnG[ServerGamemode]n[ServerGamemode][ServerGamemode]ation = a->s[ServerGamemode]cond.spawnG[ServerGamemode]n[ServerGamemode][ServerGamemode]ation;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]A[ServerGamemode]pawnX = a->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.x;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]A[ServerGamemode]pawnY = a->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.y;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]A[ServerGamemode]pawnZ = a->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.z;
    }
    if (b != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) {
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]B[ServerGamemode]pawnG[ServerGamemode]n[ServerGamemode][ServerGamemode]ation = b->s[ServerGamemode]cond.spawnG[ServerGamemode]n[ServerGamemode][ServerGamemode]ation;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]B[ServerGamemode]pawnX = b->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.x;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]B[ServerGamemode]pawnY = b->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.y;
        pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]B[ServerGamemode]pawnZ = b->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos.z;
    }
    pkt.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] ? 1 : 0;
    pkt.sco[ServerGamemode][ServerGamemode]A = d.sco[ServerGamemode][ServerGamemode]A;
    pkt.sco[ServerGamemode][ServerGamemode]B = d.sco[ServerGamemode][ServerGamemode]B;
    pkt.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] = d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode];
    pkt.co[ServerGamemode]ntdownL[ServerGamemode]ft = d.co[ServerGamemode]ntdown;
    pkt.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode];
    pkt.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft = d.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft;
    pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId = d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId;
    pkt.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId = d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId;
    pkt.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    std::st[ServerGamemode]ncpy(pkt.t[ServerGamemode]amANam[ServerGamemode], d.t[ServerGamemode]amANam[ServerGamemode].c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.t[ServerGamemode]amANam[ServerGamemode]) - 1);
    std::st[ServerGamemode]ncpy(pkt.t[ServerGamemode]amBNam[ServerGamemode], d.t[ServerGamemode]amBNam[ServerGamemode].c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.t[ServerGamemode]amBNam[ServerGamemode]) - 1);

    // ── FFA/T[ServerGamemode]M [ServerGamemode]xt[ServerGamemode]nsion fi[ServerGamemode][ServerGamemode]ds ───────────────────────────────────
    std::st[ServerGamemode]ncpy(pkt.matchMod[ServerGamemode], d.matchMod[ServerGamemode].c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.matchMod[ServerGamemode]) - 1);
    pkt.match[ServerGamemode]ta[ServerGamemode]tTick = d.match[ServerGamemode]ta[ServerGamemode]tTick;
    pkt.s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = d.c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick;
    pkt.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode];
    pkt.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
    pkt.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
    pkt.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds = d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds;
    pkt.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds = (int32_t)d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    pkt.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds = (int32_t)d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;

    // FFA top-3 [ServerGamemode][ServerGamemode]ad[ServerGamemode][ServerGamemode]boa[ServerGamemode]d
    if (d.matchMod[ServerGamemode] == "ffa") {
        // [ServerGamemode]o[ServerGamemode]t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s by ki[ServerGamemode][ServerGamemode]s d[ServerGamemode]sc[ServerGamemode]nding
        std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::pai[ServerGamemode]<[ServerGamemode]int32_t, int>> so[ServerGamemode]t[ServerGamemode]d;
        fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : d.ffaKi[ServerGamemode][ServerGamemode]s)
            so[ServerGamemode]t[ServerGamemode]d.p[ServerGamemode]sh_back({k[ServerGamemode].fi[ServerGamemode]st, k[ServerGamemode].s[ServerGamemode]cond});
        std::so[ServerGamemode]t(so[ServerGamemode]t[ServerGamemode]d.b[ServerGamemode]gin(), so[ServerGamemode]t[ServerGamemode]d.[ServerGamemode]nd(),
            [](const a[ServerGamemode]to& a, const a[ServerGamemode]to& b) { [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n a.s[ServerGamemode]cond > b.s[ServerGamemode]cond; });
        fo[ServerGamemode] (int i = 0; i < 3 && i < (int)so[ServerGamemode]t[ServerGamemode]d.siz[ServerGamemode](); ++i) {
            pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode]Ids[i] = so[ServerGamemode]t[ServerGamemode]d[i].fi[ServerGamemode]st;
            pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode][ServerGamemode]co[ServerGamemode][ServerGamemode]s[i] = so[ServerGamemode]t[ServerGamemode]d[i].s[ServerGamemode]cond;
            a[ServerGamemode]to nam[ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(so[ServerGamemode]t[ServerGamemode]d[i].fi[ServerGamemode]st);
            if (nam[ServerGamemode]It != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
                std::st[ServerGamemode]ncpy(pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode]Nam[ServerGamemode]s[i], nam[ServerGamemode]It->s[ServerGamemode]cond.nam[ServerGamemode].c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode]Nam[ServerGamemode]s[i]) - 1);
            [ServerGamemode][ServerGamemode]s[ServerGamemode]
                std::snp[ServerGamemode]intf(pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode]Nam[ServerGamemode]s[i], siz[ServerGamemode]of(pkt.ffaL[ServerGamemode]ad[ServerGamemode][ServerGamemode]Nam[ServerGamemode]s[i]),
                              "NPC %[ServerGamemode]", so[ServerGamemode]t[ServerGamemode]d[i].fi[ServerGamemode]st);
        }
    }

    // Pa[ServerGamemode]ticipant I[ServerGamemode]s and t[ServerGamemode]ams
    pkt.pa[ServerGamemode]ticipantCo[ServerGamemode]nt = ([ServerGamemode]int8_t)std::min((siz[ServerGamemode]_t)32, d.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
    fo[ServerGamemode] ([ServerGamemode]int8_t i = 0; i < pkt.pa[ServerGamemode]ticipantCo[ServerGamemode]nt; ++i) {
        pkt.pa[ServerGamemode]ticipantIds[i] = d.pa[ServerGamemode]ticipants[i];
        a[ServerGamemode]to t[ServerGamemode]amIt = d.matchT[ServerGamemode]ams.find(d.pa[ServerGamemode]ticipants[i]);
        pkt.pa[ServerGamemode]ticipantT[ServerGamemode]ams[i] = t[ServerGamemode]amIt != d.matchT[ServerGamemode]ams.[ServerGamemode]nd() ? ([ServerGamemode]int8_t)t[ServerGamemode]amIt->s[ServerGamemode]cond : 0xFF;
    }

    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            contin[ServerGamemode][ServerGamemode];
        const [ServerGamemode]int32_t [ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
        const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
            sock, const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond), &pkt, siz[ServerGamemode]of(pkt), [ServerGamemode][ServerGamemode][ServerGamemode]ntId,
            [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond)), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        const boo[ServerGamemode] s[ServerGamemode]nt = [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t == R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t::Q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d;
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]] s[ServerGamemode]nt d[ServerGamemode][ServerGamemode][ServerGamemode] stat[ServerGamemode] d[ServerGamemode][ServerGamemode][ServerGamemode]Id=%[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode]sion=%[ServerGamemode] phas[ServerGamemode]=%[ServerGamemode] mod[ServerGamemode]=%s map=%s p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] s[ServerGamemode]nt=%d sco[ServerGamemode][ServerGamemode]=%d-%d [ServerGamemode][ServerGamemode]d=%d b[ServerGamemode][ServerGamemode][ServerGamemode]=%d\n",
            d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id, d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion, ([ServerGamemode]nsign[ServerGamemode]d)d.phas[ServerGamemode], d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.mapId.c_st[ServerGamemode](),
            k[ServerGamemode].s[ServerGamemode]cond.id, (int)s[ServerGamemode]nt, d.sco[ServerGamemode][ServerGamemode]A, d.sco[ServerGamemode][ServerGamemode]B, d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s, d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s);
    }
}

// Pick ONE [ServerGamemode]andom map spawn point as th[ServerGamemode] match ancho[ServerGamemode]. Both t[ServerGamemode]ams a[ServerGamemode]ways
// spawn n[ServerGamemode]a[ServerGamemode] this sing[ServerGamemode][ServerGamemode] point (with a f[ServerGamemode][ServerGamemode]sh [ServerGamemode]andom XY offs[ServerGamemode]t [ServerGamemode]ach spawn), so
// [ServerGamemode][ServerGamemode]spawns [ServerGamemode]and [ServerGamemode]ight back in th[ServerGamemode] fight — max action, no map [ServerGamemode]diting n[ServerGamemode][ServerGamemode]d[ServerGamemode]d.
[ServerGamemode]oid assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d, const H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d)
{
    d.spawnsAssign[ServerGamemode]d = t[ServerGamemode][ServerGamemode][ServerGamemode];
    if (!wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty())
    {
        std::mt19937 [ServerGamemode]ng(std::[ServerGamemode]andom_d[ServerGamemode][ServerGamemode]ic[ServerGamemode]{}());
        std::[ServerGamemode]nifo[ServerGamemode]m_int_dist[ServerGamemode]ib[ServerGamemode]tion<siz[ServerGamemode]_t> dist(0, wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]() - 1);
        const siz[ServerGamemode]_t ancho[ServerGamemode]Ind[ServerGamemode]x = dist([ServerGamemode]ng);
        d.spawnAncho[ServerGamemode]Ind[ServerGamemode]x = ([ServerGamemode]int32_t)ancho[ServerGamemode]Ind[ServerGamemode]x;
        ++d.spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
        const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 ancho[ServerGamemode] = wo[ServerGamemode][ServerGamemode]d.spawnPoints[ancho[ServerGamemode]Ind[ServerGamemode]x].position;
        d.spawnA = ancho[ServerGamemode];
        d.spawnB = ancho[ServerGamemode];
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Ancho[ServerGamemode]] map=%s ancho[ServerGamemode]Ind[ServerGamemode]x=%z[ServerGamemode] ancho[ServerGamemode]=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_st[ServerGamemode](), ancho[ServerGamemode]Ind[ServerGamemode]x, ancho[ServerGamemode].x, ancho[ServerGamemode].y, ancho[ServerGamemode].z);
    }
    [ServerGamemode][ServerGamemode]s[ServerGamemode]
    {
        d.spawnA = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(1.0f, 5.0f, 30.0f);
        d.spawnB = d.spawnA;
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Fa[ServerGamemode][ServerGamemode]back] map=%s [ServerGamemode][ServerGamemode]ason=no_spawn_points fina[ServerGamemode]=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_st[ServerGamemode](), d.spawnA.x, d.spawnA.y, d.spawnA.z);
    }
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] ancho[ServerGamemode]=(%.1f %.1f %.1f) spawns=%z[ServerGamemode]\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]());
}

// Th[ServerGamemode] ancho[ServerGamemode] p[ServerGamemode][ServerGamemode]s a [ServerGamemode]andom XY offs[ServerGamemode]t (so nobody can p[ServerGamemode][ServerGamemode]dict th[ServerGamemode] [ServerGamemode]xact spot).
g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d)
{
    static std::mt19937 [ServerGamemode]ng(std::[ServerGamemode]andom_d[ServerGamemode][ServerGamemode]ic[ServerGamemode]{}());
    std::[ServerGamemode]nifo[ServerGamemode]m_[ServerGamemode][ServerGamemode]a[ServerGamemode]_dist[ServerGamemode]ib[ServerGamemode]tion<f[ServerGamemode]oat> dist(-d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s, d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s);
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n d.spawnA + g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(dist([ServerGamemode]ng), dist([ServerGamemode]ng), 0.0f);
}

[ServerGamemode]oid assignGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipants([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                    const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId = 0;
    d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId = 0;
    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
    {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            contin[ServerGamemode][ServerGamemode];
        if (d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId == 0)
            d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId = k[ServerGamemode].fi[ServerGamemode]st;
        [ServerGamemode][ServerGamemode]s[ServerGamemode]
            d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId = k[ServerGamemode].fi[ServerGamemode]st;
    }
}

// P[ServerGamemode]ac[ServerGamemode] both d[ServerGamemode][ServerGamemode][ServerGamemode]ists n[ServerGamemode]a[ServerGamemode] th[ServerGamemode] match ancho[ServerGamemode] with f[ServerGamemode][ServerGamemode][ServerGamemode] HP and f[ServerGamemode][ServerGamemode][ServerGamemode] ammo.
[ServerGamemode]oid t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                              std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    a[ServerGamemode]to p[ServerGamemode]ac[ServerGamemode] = [&]([ServerGamemode]int32_t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id)
    {
        a[ServerGamemode]to it = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
        if (it == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p = it->s[ServerGamemode]cond;
        const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 spawn = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        p.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = spawn;
        p.has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = t[ServerGamemode][ServerGamemode][ServerGamemode];
        const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 offs[ServerGamemode]t = spawn - d.spawnA;
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawn] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] map=%s ancho[ServerGamemode]=(%.3f,%.3f,%.3f) offs[ServerGamemode]t=(%.3f,%.3f,%.3f) fina[ServerGamemode]=(%.3f,%.3f,%.3f)\n",
            p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, d.mapId.c_st[ServerGamemode](), d.spawnA.x, d.spawnA.y, d.spawnA.z,
            offs[ServerGamemode]t.x, offs[ServerGamemode]t.y, offs[ServerGamemode]t.z, spawn.x, spawn.y, spawn.z);
        p.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
        if (!p.d[ServerGamemode]ad)
        {
            b[ServerGamemode]ginA[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode]T[ServerGamemode]ansfo[ServerGamemode]m(p, spawn, g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f), p.yaw, "d[ServerGamemode][ServerGamemode][ServerGamemode]-spawn");
            p.j[ServerGamemode]stR[ServerGamemode]spawn[ServerGamemode]d = t[ServerGamemode][ServerGamemode][ServerGamemode];
        }
    };
    p[ServerGamemode]ac[ServerGamemode](d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId);
    p[ServerGamemode]ac[ServerGamemode](d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId);
}

[ServerGamemode]oid b[ServerGamemode]ginGam[ServerGamemode]mod[ServerGamemode]Co[ServerGamemode]ntdown([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                        std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    ++d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
    ++d.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode];
    ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.sco[ServerGamemode][ServerGamemode]A = 0;
    d.sco[ServerGamemode][ServerGamemode]B = 0;
    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_COUNT[ServerGamemode]OWN;
    d.co[ServerGamemode]ntdown = d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds;
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]] s[ServerGamemode][ServerGamemode][ServerGamemode]ct[ServerGamemode]d a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] map=%s d[ServerGamemode][ServerGamemode][ServerGamemode]Id=%[ServerGamemode] stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion=%[ServerGamemode]\n",
        d.mapId.c_st[ServerGamemode](), d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id, d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion);
    t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] co[ServerGamemode]ntdown sta[ServerGamemode]t[ServerGamemode]d p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s=%[ServerGamemode]/%[ServerGamemode]\n", d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId, d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId);
}

// [ServerGamemode]oadH[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d app[ServerGamemode]nds, so c[ServerGamemode][ServerGamemode]a[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ything it pop[ServerGamemode][ServerGamemode]at[ServerGamemode]s b[ServerGamemode]fo[ServerGamemode][ServerGamemode] a [ServerGamemode][ServerGamemode][ServerGamemode]oad.
[ServerGamemode]oid c[ServerGamemode][ServerGamemode]a[ServerGamemode]H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d(H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d)
{
    wo[ServerGamemode][ServerGamemode]d.t[ServerGamemode]iang[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    wo[ServerGamemode][ServerGamemode]d.bo[ServerGamemode]ndsMin = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f);
    wo[ServerGamemode][ServerGamemode]d.bo[ServerGamemode]ndsMax = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f);
    wo[ServerGamemode][ServerGamemode]d.spawnPoints.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    wo[ServerGamemode][ServerGamemode]d.co[ServerGamemode][ServerGamemode]isionCh[ServerGamemode]nks.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    wo[ServerGamemode][ServerGamemode]d.co[ServerGamemode][ServerGamemode]isionLa[ServerGamemode]g[ServerGamemode]T[ServerGamemode]iang[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    wo[ServerGamemode][ServerGamemode]d.co[ServerGamemode][ServerGamemode]ision[ServerGamemode][ServerGamemode]bG[ServerGamemode]ids.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
}

[ServerGamemode]oid b[ServerGamemode]oadcastMapChang[ServerGamemode]([ServerGamemode]OCKET sock,
                        const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                        const std::st[ServerGamemode]ing& mapId,
                        const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                        [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    MapChang[ServerGamemode]Pack[ServerGamemode]t pkt{};
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_MAP_CHANGE;
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].tick = 0;
    std::st[ServerGamemode]ncpy(pkt.mapId, mapId.c_st[ServerGamemode](), siz[ServerGamemode]of(pkt.mapId) - 1);
    pkt.d[ServerGamemode][ServerGamemode][ServerGamemode]Id = d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
    pkt.mapV[ServerGamemode][ServerGamemode]sion = d.mapV[ServerGamemode][ServerGamemode]sion;
    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
    {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            contin[ServerGamemode][ServerGamemode];
        const [ServerGamemode]int32_t [ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
        const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
            sock, const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond), &pkt, siz[ServerGamemode]of(pkt), [ServerGamemode][ServerGamemode][ServerGamemode]ntId,
            [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond)), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        const boo[ServerGamemode] s[ServerGamemode]nt = [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t == R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t::Q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d;
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Map] s[ServerGamemode]nd map=%s p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]=1 s[ServerGamemode]nt=%d [ServerGamemode][ServerGamemode][ServerGamemode]sion=%[ServerGamemode]\n",
            mapId.c_st[ServerGamemode](), k[ServerGamemode].s[ServerGamemode]cond.id, (int)s[ServerGamemode]nt, d.mapV[ServerGamemode][ServerGamemode]sion);
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Pack[ServerGamemode]t[ServerGamemode][ServerGamemode]nd] typ[ServerGamemode]=MapChang[ServerGamemode]Pack[ServerGamemode]t [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]=1 p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] s[ServerGamemode]nt=%d map=%s\n",
            k[ServerGamemode].s[ServerGamemode]cond.id, (int)s[ServerGamemode]nt, mapId.c_st[ServerGamemode]());
        if (s[ServerGamemode]nt)
            ++tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t;
    }
}

[ServerGamemode]oid b[ServerGamemode]oadcastComm[ServerGamemode]nityNotification(
    [ServerGamemode]OCKET sock,
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
    const std::st[ServerGamemode]ing& m[ServerGamemode]ssag[ServerGamemode],
    [ServerGamemode]int16_t d[ServerGamemode][ServerGamemode]ationTicks,
    [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]NotificationPack[ServerGamemode]t pack[ServerGamemode]t{};
    pack[ServerGamemode]t.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_[ServerGamemode]ERVER_NOTIFICATION;
    pack[ServerGamemode]t.[ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
    pack[ServerGamemode]t.[ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionId = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]E[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionId();
    pack[ServerGamemode]t.d[ServerGamemode][ServerGamemode]ationTicks = d[ServerGamemode][ServerGamemode]ationTicks;
    std::st[ServerGamemode]ncpy(pack[ServerGamemode]t.tit[ServerGamemode][ServerGamemode], "MiMITA [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]", siz[ServerGamemode]of(pack[ServerGamemode]t.tit[ServerGamemode][ServerGamemode]) - 1);
    std::st[ServerGamemode]ncpy(pack[ServerGamemode]t.m[ServerGamemode]ssag[ServerGamemode], m[ServerGamemode]ssag[ServerGamemode].c_st[ServerGamemode](), siz[ServerGamemode]of(pack[ServerGamemode]t.m[ServerGamemode]ssag[ServerGamemode]) - 1);
    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) contin[ServerGamemode][ServerGamemode];
        const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
            sock, k[ServerGamemode].s[ServerGamemode]cond, &pack[ServerGamemode]t, siz[ServerGamemode]of(pack[ServerGamemode]t), pack[ServerGamemode]t.[ServerGamemode][ServerGamemode][ServerGamemode]ntId,
            [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](k[ServerGamemode].s[ServerGamemode]cond), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
            "[[ServerGamemode]ERVER NOTIFICATION [ServerGamemode]EN[ServerGamemode]] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d=%d m[ServerGamemode]ssag[ServerGamemode]=%s\n",
            k[ServerGamemode].s[ServerGamemode]cond.id, [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t == R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t::Q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d,
            m[ServerGamemode]ssag[ServerGamemode].c_st[ServerGamemode]());
    }
}

// Load a map into a f[ServerGamemode][ServerGamemode]sh t[ServerGamemode]mp wo[ServerGamemode][ServerGamemode]d; t[ServerGamemode][ServerGamemode][ServerGamemode] on[ServerGamemode]y if it [ServerGamemode]oads AN[ServerGamemode] has [ServerGamemode][ServerGamemode]a[ServerGamemode]
// spawn points, so p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s a[ServerGamemode]ways ancho[ServerGamemode] at spawn points, n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] [ServerGamemode]nd[ServerGamemode][ServerGamemode] th[ServerGamemode] map.
boo[ServerGamemode] t[ServerGamemode]yLoad[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Map(const std::st[ServerGamemode]ing& mapId, H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& o[ServerGamemode]t)
{
    const std::st[ServerGamemode]ing path = "ass[ServerGamemode]ts/maps/" + mapId + ".g[ServerGamemode]b";
    H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d candidat[ServerGamemode];
    if (![ServerGamemode]oadH[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d(path.c_st[ServerGamemode](), candidat[ServerGamemode]))
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode];
    if (candidat[ServerGamemode].spawnPoints.[ServerGamemode]mpty())
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode];
    o[ServerGamemode]t = std::mo[ServerGamemode][ServerGamemode](candidat[ServerGamemode]);
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n t[ServerGamemode][ServerGamemode][ServerGamemode];
}

// Mo[ServerGamemode][ServerGamemode] a [ServerGamemode]oad[ServerGamemode]d t[ServerGamemode]mp wo[ServerGamemode][ServerGamemode]d into th[ServerGamemode] [ServerGamemode]i[ServerGamemode][ServerGamemode] wo[ServerGamemode][ServerGamemode]d + NPC co[ServerGamemode][ServerGamemode]ision wo[ServerGamemode][ServerGamemode]d.
[ServerGamemode]oid commitGam[ServerGamemode]mod[ServerGamemode]Map([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d, H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d, Wo[ServerGamemode][ServerGamemode]d& npcWo[ServerGamemode][ServerGamemode]d,
                   H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& tmp, const std::st[ServerGamemode]ing& mapId)
{
    c[ServerGamemode][ServerGamemode]a[ServerGamemode]H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d(wo[ServerGamemode][ServerGamemode]d);
    wo[ServerGamemode][ServerGamemode]d = std::mo[ServerGamemode][ServerGamemode](tmp);
    b[ServerGamemode]i[ServerGamemode]dNpcWo[ServerGamemode][ServerGamemode]dCo[ServerGamemode][ServerGamemode]ision(npcWo[ServerGamemode][ServerGamemode]d, wo[ServerGamemode][ServerGamemode]d);
    s[ServerGamemode]t[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]MapId(mapId);
    d.mapId = mapId;
    ++d.mapV[ServerGamemode][ServerGamemode]sion;
    ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
}

// R[ServerGamemode][ServerGamemode]oad th[ServerGamemode] wo[ServerGamemode][ServerGamemode]d fo[ServerGamemode] a chos[ServerGamemode]n map (chang[ServerGamemode]map / [ServerGamemode]otation commit). On[ServerGamemode]y [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]
// to[ServerGamemode]ch[ServerGamemode]s th[ServerGamemode] [ServerGamemode]i[ServerGamemode][ServerGamemode] wo[ServerGamemode][ServerGamemode]d aft[ServerGamemode][ServerGamemode] th[ServerGamemode] n[ServerGamemode]w map is confi[ServerGamemode]m[ServerGamemode]d [ServerGamemode]oad[ServerGamemode]d, so a fai[ServerGamemode][ServerGamemode]d
// swap n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] [ServerGamemode]mpti[ServerGamemode]s th[ServerGamemode] wo[ServerGamemode][ServerGamemode]d (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] fa[ServerGamemode][ServerGamemode] [ServerGamemode]nd[ServerGamemode][ServerGamemode] it).
boo[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode]oadGam[ServerGamemode]mod[ServerGamemode]Map([ServerGamemode]OCKET sock,
                   [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                   std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                   H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                   Wo[ServerGamemode][ServerGamemode]d& npcWo[ServerGamemode][ServerGamemode]d,
                   const std::st[ServerGamemode]ing& mapId,
                   [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d tmp;
    if (!t[ServerGamemode]yLoad[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Map(mapId, tmp))
    {
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode][ServerGamemode][ServerGamemode]o[ServerGamemode]([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] map swap fai[ServerGamemode][ServerGamemode]d o[ServerGamemode] has no spawn points: %s\n", mapId.c_st[ServerGamemode]());
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode];
    }
    commitGam[ServerGamemode]mod[ServerGamemode]Map(d, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tmp, mapId);
    assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
    b[ServerGamemode]oadcastMapChang[ServerGamemode](sock, d, mapId, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
    t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
    // Map chang[ServerGamemode]s a[ServerGamemode][ServerGamemode] acto[ServerGamemode] [ServerGamemode]if[ServerGamemode]cyc[ServerGamemode][ServerGamemode] bo[ServerGamemode]nda[ServerGamemode]i[ServerGamemode]s, not d[ServerGamemode][ServerGamemode][ServerGamemode]-on[ServerGamemode]y t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]ts.
    // E[ServerGamemode][ServerGamemode][ServerGamemode]y acti[ServerGamemode][ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]c[ServerGamemode]i[ServerGamemode][ServerGamemode]s a f[ServerGamemode][ServerGamemode]sh a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] spawn on th[ServerGamemode] n[ServerGamemode]w map.
    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p = k[ServerGamemode].s[ServerGamemode]cond;
        if (p.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) contin[ServerGamemode][ServerGamemode];
        p.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        p.has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = t[ServerGamemode][ServerGamemode][ServerGamemode];
        b[ServerGamemode]ginA[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode]T[ServerGamemode]ansfo[ServerGamemode]m(p, p.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos, g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f), p.yaw,
                                    "map-chang[ServerGamemode]-[ServerGamemode][ServerGamemode]spawn");
        comp[ServerGamemode][ServerGamemode]t[ServerGamemode]A[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode][ServerGamemode]pawn(sock, p, fa[ServerGamemode]s[ServerGamemode]);
    }
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] map chang[ServerGamemode]d [ServerGamemode]i[ServerGamemode][ServerGamemode] to %s (spawns=%z[ServerGamemode])\n",
        mapId.c_st[ServerGamemode](), wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]());
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n t[ServerGamemode][ServerGamemode][ServerGamemode];
}

// Ro[ServerGamemode]nd-[ServerGamemode]obin [ServerGamemode]otation: pick a map not [ServerGamemode]s[ServerGamemode]d this cyc[ServerGamemode][ServerGamemode] (n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] th[ServerGamemode] on[ServerGamemode] j[ServerGamemode]st
// p[ServerGamemode]ay[ServerGamemode]d), skip maps that fai[ServerGamemode] to [ServerGamemode]oad o[ServerGamemode] ha[ServerGamemode][ServerGamemode] no spawn points, and cyc[ServerGamemode][ServerGamemode] th[ServerGamemode]
// who[ServerGamemode][ServerGamemode] poo[ServerGamemode] b[ServerGamemode]fo[ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]p[ServerGamemode]ating. R[ServerGamemode]t[ServerGamemode][ServerGamemode]ns t[ServerGamemode][ServerGamemode][ServerGamemode] if th[ServerGamemode] map chang[ServerGamemode]d.
boo[ServerGamemode] [ServerGamemode]otat[ServerGamemode]ToN[ServerGamemode]xtGam[ServerGamemode]mod[ServerGamemode]Map([ServerGamemode]OCKET sock,
                         [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                         std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                         H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                         Wo[ServerGamemode][ServerGamemode]d& npcWo[ServerGamemode][ServerGamemode]d,
                         [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    if (d.mapPoo[ServerGamemode].siz[ServerGamemode]() <= 1)
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode];

    a[ServerGamemode]to [ServerGamemode]n[ServerGamemode]s[ServerGamemode]dCandidat[ServerGamemode]s = [&]() {
        std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing> [ServerGamemode];
        fo[ServerGamemode] (const a[ServerGamemode]to& m : d.mapPoo[ServerGamemode])
            if (!d.[ServerGamemode]s[ServerGamemode]dMaps.co[ServerGamemode]nt(m) && m != d.mapId)
                [ServerGamemode].p[ServerGamemode]sh_back(m);
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n [ServerGamemode];
    };

    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing> candidat[ServerGamemode]s = [ServerGamemode]n[ServerGamemode]s[ServerGamemode]dCandidat[ServerGamemode]s();
    if (candidat[ServerGamemode]s.[ServerGamemode]mpty())
    {
        // Who[ServerGamemode][ServerGamemode] poo[ServerGamemode] [ServerGamemode]s[ServerGamemode]d this cyc[ServerGamemode][ServerGamemode] — sta[ServerGamemode]t f[ServerGamemode][ServerGamemode]sh, sti[ServerGamemode][ServerGamemode] a[ServerGamemode]oiding th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt map.
        d.[ServerGamemode]s[ServerGamemode]dMaps.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
        d.[ServerGamemode]s[ServerGamemode]dMaps.ins[ServerGamemode][ServerGamemode]t(d.mapId);
        candidat[ServerGamemode]s = [ServerGamemode]n[ServerGamemode]s[ServerGamemode]dCandidat[ServerGamemode]s();
    }

    std::mt19937 [ServerGamemode]ng(std::[ServerGamemode]andom_d[ServerGamemode][ServerGamemode]ic[ServerGamemode]{}());
    std::sh[ServerGamemode]ff[ServerGamemode][ServerGamemode](candidat[ServerGamemode]s.b[ServerGamemode]gin(), candidat[ServerGamemode]s.[ServerGamemode]nd(), [ServerGamemode]ng);

    fo[ServerGamemode] (const std::st[ServerGamemode]ing& cand : candidat[ServerGamemode]s)
    {
        H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d tmp;
        if (t[ServerGamemode]yLoad[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Map(cand, tmp))
        {
            d.[ServerGamemode]s[ServerGamemode]dMaps.ins[ServerGamemode][ServerGamemode]t(cand);
            commitGam[ServerGamemode]mod[ServerGamemode]Map(d, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tmp, cand);
            assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
            b[ServerGamemode]oadcastMapChang[ServerGamemode](sock, d, cand, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] [ServerGamemode]otat[ServerGamemode]d to map %s (spawns=%z[ServerGamemode])\n",
                cand.c_st[ServerGamemode](), wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]());
            [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n t[ServerGamemode][ServerGamemode][ServerGamemode];
        }
        // Fai[ServerGamemode][ServerGamemode]d to [ServerGamemode]oad o[ServerGamemode] has no spawn points — skip it this cyc[ServerGamemode][ServerGamemode].
        d.[ServerGamemode]s[ServerGamemode]dMaps.ins[ServerGamemode][ServerGamemode]t(cand);
    }
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode]; // nothing [ServerGamemode]a[ServerGamemode]id — k[ServerGamemode][ServerGamemode]p th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt map
}

// ── FFA/T[ServerGamemode]M match h[ServerGamemode][ServerGamemode]p[ServerGamemode][ServerGamemode]s ───────────────────────────────────────────────

[ServerGamemode]oid assignMatchPa[ServerGamemode]ticipants([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                             std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                             std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>* npcs = n[ServerGamemode][ServerGamemode][ServerGamemode]pt[ServerGamemode])
{
    d.pa[ServerGamemode]ticipants.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.ffaKi[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.ffa[ServerGamemode][ServerGamemode]aths.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.matchT[ServerGamemode]ams.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;

    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] == [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) {
            d.pa[ServerGamemode]ticipants.p[ServerGamemode]sh_back(k[ServerGamemode].fi[ServerGamemode]st);
            d.ffaKi[ServerGamemode][ServerGamemode]s[k[ServerGamemode].fi[ServerGamemode]st] = 0;
            d.ffa[ServerGamemode][ServerGamemode]aths[k[ServerGamemode].fi[ServerGamemode]st] = 0;
        }
    }

    if (npcs) {
        fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : *npcs) {
            if (k[ServerGamemode].s[ServerGamemode]cond.h[ServerGamemode]a[ServerGamemode]th <= 0) contin[ServerGamemode][ServerGamemode];
            d.pa[ServerGamemode]ticipants.p[ServerGamemode]sh_back(k[ServerGamemode].fi[ServerGamemode]st);
            d.ffaKi[ServerGamemode][ServerGamemode]s[k[ServerGamemode].fi[ServerGamemode]st] = 0;
            d.ffa[ServerGamemode][ServerGamemode]aths[k[ServerGamemode].fi[ServerGamemode]st] = 0;
        }
    }

    // [ServerGamemode]o[ServerGamemode]t by I[ServerGamemode] fo[ServerGamemode] d[ServerGamemode]t[ServerGamemode][ServerGamemode]ministic t[ServerGamemode]am assignm[ServerGamemode]nt
    std::so[ServerGamemode]t(d.pa[ServerGamemode]ticipants.b[ServerGamemode]gin(), d.pa[ServerGamemode]ticipants.[ServerGamemode]nd());

    if (d.matchMod[ServerGamemode] == "tdm") {
        fo[ServerGamemode] (siz[ServerGamemode]_t i = 0; i < d.pa[ServerGamemode]ticipants.siz[ServerGamemode](); ++i) {
            d.matchT[ServerGamemode]ams[d.pa[ServerGamemode]ticipants[i]] = (int)(i % 2);
            a[ServerGamemode]to p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.pa[ServerGamemode]ticipants[i]);
            if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
                p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.matchT[ServerGamemode]am = (int)(i % 2);
            if (npcs) {
                a[ServerGamemode]to npcIt = npcs->find(d.pa[ServerGamemode]ticipants[i]);
                if (npcIt != npcs->[ServerGamemode]nd())
                    npcIt->s[ServerGamemode]cond.matchT[ServerGamemode]am = (int)(i % 2);
            }
        }
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[FFA/T[ServerGamemode]M] Assign[ServerGamemode]d %z[ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s to t[ServerGamemode]ams ([ServerGamemode][ServerGamemode]d=%d b[ServerGamemode][ServerGamemode][ServerGamemode]=%d)\n",
            d.pa[ServerGamemode]ticipants.siz[ServerGamemode](), d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s, d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s);
    }
}

[ServerGamemode]oid t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tA[ServerGamemode][ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                                     std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    fo[ServerGamemode] ([ServerGamemode]int32_t pid : d.pa[ServerGamemode]ticipants) {
        a[ServerGamemode]to it = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(pid);
        if (it == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) contin[ServerGamemode][ServerGamemode];
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p = it->s[ServerGamemode]cond;
        const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 spawn = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        p.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = spawn;
        p.has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = t[ServerGamemode][ServerGamemode][ServerGamemode];
        p.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
        if (!p.d[ServerGamemode]ad) {
            b[ServerGamemode]ginA[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode]T[ServerGamemode]ansfo[ServerGamemode]m(p, spawn, g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f), p.yaw, "match-spawn");
            p.j[ServerGamemode]stR[ServerGamemode]spawn[ServerGamemode]d = t[ServerGamemode][ServerGamemode][ServerGamemode];
        }
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[Match[ServerGamemode]pawn] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] spawn=(%.3f,%.3f,%.3f)\n",
            pid, spawn.x, spawn.y, spawn.z);
    }
}

[ServerGamemode]oid [ServerGamemode][ServerGamemode]spawnA[ServerGamemode][ServerGamemode]Pa[ServerGamemode]ticipants([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                            std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    fo[ServerGamemode] ([ServerGamemode]int32_t pid : d.pa[ServerGamemode]ticipants) {
        a[ServerGamemode]to it = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(pid);
        if (it == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) contin[ServerGamemode][ServerGamemode];
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p = it->s[ServerGamemode]cond;
        p.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        p.has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = t[ServerGamemode][ServerGamemode][ServerGamemode];
        p.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
    }
}

[ServerGamemode]oid [ServerGamemode][ServerGamemode]s[ServerGamemode]tMatch[ServerGamemode]co[ServerGamemode][ServerGamemode]s([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d)
{
    d.sco[ServerGamemode][ServerGamemode]A = 0;
    d.sco[ServerGamemode][ServerGamemode]B = 0;
    d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    d.ffaKi[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    d.ffa[ServerGamemode][ServerGamemode]aths.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
    fo[ServerGamemode] ([ServerGamemode]int32_t pid : d.pa[ServerGamemode]ticipants) {
        d.ffaKi[ServerGamemode][ServerGamemode]s[pid] = 0;
        d.ffa[ServerGamemode][ServerGamemode]aths[pid] = 0;
    }
}

[ServerGamemode]oid b[ServerGamemode]ginMatchCo[ServerGamemode]ntdown([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                         std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                         [ServerGamemode]int32_t c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ntTick)
{
    ++d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
    ++d.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode];
    ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = -1;
    d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 0;
    d.co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tTick = c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ntTick;
    d.match[ServerGamemode]ta[ServerGamemode]tTick = c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ntTick + ([ServerGamemode]int32_t)(d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds * 60.0f);
    d.co[ServerGamemode]ntdown = d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds;
    d.matchTim[ServerGamemode]LimitTick = 0;
    [ServerGamemode][ServerGamemode]s[ServerGamemode]tMatch[ServerGamemode]co[ServerGamemode][ServerGamemode]s(d);
    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_COUNT[ServerGamemode]OWN;
    t[ServerGamemode][ServerGamemode][ServerGamemode]po[ServerGamemode]tA[ServerGamemode][ServerGamemode]Pa[ServerGamemode]ticipantsTo[ServerGamemode]pawns(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Match] co[ServerGamemode]ntdown sta[ServerGamemode]t[ServerGamemode]d mod[ServerGamemode]=%s d[ServerGamemode][ServerGamemode][ServerGamemode]Id=%[ServerGamemode] match[ServerGamemode]ta[ServerGamemode]tTick=%[ServerGamemode] tim[ServerGamemode]LimitTick=%[ServerGamemode] pa[ServerGamemode]ticipants=%z[ServerGamemode]\n",
        d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id, d.match[ServerGamemode]ta[ServerGamemode]tTick, d.matchTim[ServerGamemode]LimitTick, d.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
}

static [ServerGamemode]oid [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d, [ServerGamemode]int32_t tick,
                                      const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    P[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]MatchE[ServerGamemode][ServerGamemode]nt [ServerGamemode][ServerGamemode][ServerGamemode]nt;
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.[ServerGamemode][ServerGamemode][ServerGamemode]ntId = "match_" + std::to_st[ServerGamemode]ing(tick) + "_" + std::to_st[ServerGamemode]ing(d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id);
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.matchId = "match_" + std::to_st[ServerGamemode]ing(d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id);
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.mod[ServerGamemode] = d.matchMod[ServerGamemode];
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] == 0 ? "sco[ServerGamemode][ServerGamemode]_[ServerGamemode]imit" : "tim[ServerGamemode]_[ServerGamemode]imit";
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.[ServerGamemode][ServerGamemode]d[ServerGamemode]co[ServerGamemode][ServerGamemode] = d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.b[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]co[ServerGamemode][ServerGamemode] = d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am == 0 ? "[ServerGamemode][ServerGamemode]d" : "b[ServerGamemode][ServerGamemode][ServerGamemode]";
    [ServerGamemode][ServerGamemode][ServerGamemode]nt.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = (int64_t)d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;

    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) contin[ServerGamemode][ServerGamemode];
        P[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]MatchPa[ServerGamemode]ticipant p;
        p.[ServerGamemode]s[ServerGamemode][ServerGamemode]Id = k[ServerGamemode].s[ServerGamemode]cond.acco[ServerGamemode]ntId > 0 ? (int64_t)k[ServerGamemode].s[ServerGamemode]cond.acco[ServerGamemode]ntId : 0;
        p.[ServerGamemode]s[ServerGamemode][ServerGamemode]nam[ServerGamemode] = k[ServerGamemode].s[ServerGamemode]cond.nam[ServerGamemode];
        a[ServerGamemode]to t[ServerGamemode]amIt = d.matchT[ServerGamemode]ams.find(k[ServerGamemode].fi[ServerGamemode]st);
        p.t[ServerGamemode]am = (t[ServerGamemode]amIt != d.matchT[ServerGamemode]ams.[ServerGamemode]nd() && t[ServerGamemode]amIt->s[ServerGamemode]cond == 0) ? "[ServerGamemode][ServerGamemode]d" : "b[ServerGamemode][ServerGamemode][ServerGamemode]";
        p.ki[ServerGamemode][ServerGamemode]s = k[ServerGamemode].s[ServerGamemode]cond.ki[ServerGamemode][ServerGamemode]s;
        p.d[ServerGamemode]aths = k[ServerGamemode].s[ServerGamemode]cond.d[ServerGamemode]aths;

        if (d.matchMod[ServerGamemode] == "ffa") {
            a[ServerGamemode]to ki[ServerGamemode][ServerGamemode]It = d.ffaKi[ServerGamemode][ServerGamemode]s.find(k[ServerGamemode].fi[ServerGamemode]st);
            p.ki[ServerGamemode][ServerGamemode]s = ki[ServerGamemode][ServerGamemode]It != d.ffaKi[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ? ki[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond : 0;
            a[ServerGamemode]to d[ServerGamemode]athIt = d.ffa[ServerGamemode][ServerGamemode]aths.find(k[ServerGamemode].fi[ServerGamemode]st);
            p.d[ServerGamemode]aths = d[ServerGamemode]athIt != d.ffa[ServerGamemode][ServerGamemode]aths.[ServerGamemode]nd() ? d[ServerGamemode]athIt->s[ServerGamemode]cond : 0;
            p.won = (k[ServerGamemode].fi[ServerGamemode]st == d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "tdm") {
            a[ServerGamemode]to ki[ServerGamemode][ServerGamemode]It = d.ffaKi[ServerGamemode][ServerGamemode]s.find(k[ServerGamemode].fi[ServerGamemode]st);
            p.ki[ServerGamemode][ServerGamemode]s = ki[ServerGamemode][ServerGamemode]It != d.ffaKi[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ? ki[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond : 0;
            a[ServerGamemode]to d[ServerGamemode]athIt = d.ffa[ServerGamemode][ServerGamemode]aths.find(k[ServerGamemode].fi[ServerGamemode]st);
            p.d[ServerGamemode]aths = d[ServerGamemode]athIt != d.ffa[ServerGamemode][ServerGamemode]aths.[ServerGamemode]nd() ? d[ServerGamemode]athIt->s[ServerGamemode]cond : 0;
            p.won = (p.t[ServerGamemode]am == [ServerGamemode][ServerGamemode][ServerGamemode]nt.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am);
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] {
            p.won = (k[ServerGamemode].fi[ServerGamemode]st == d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
        }
        [ServerGamemode][ServerGamemode][ServerGamemode]nt.pa[ServerGamemode]ticipants.p[ServerGamemode]sh_back(std::mo[ServerGamemode][ServerGamemode](p));
    }

    P[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode]Q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]::instanc[ServerGamemode]().[ServerGamemode]nq[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]MatchR[ServerGamemode]s[ServerGamemode][ServerGamemode]t([ServerGamemode][ServerGamemode][ServerGamemode]nt);
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[PER[ServerGamemode]I[ServerGamemode]TENCE] Match [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode]mitt[ServerGamemode]d: mod[ServerGamemode]=%s winn[ServerGamemode][ServerGamemode]=%s pa[ServerGamemode]ticipants=%z[ServerGamemode]\n",
        [ServerGamemode][ServerGamemode][ServerGamemode]nt.mod[ServerGamemode].c_st[ServerGamemode](),
        [ServerGamemode][ServerGamemode][ServerGamemode]nt.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am.[ServerGamemode]mpty() ? std::to_st[ServerGamemode]ing(d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id).c_st[ServerGamemode]() : [ServerGamemode][ServerGamemode][ServerGamemode]nt.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am.c_st[ServerGamemode](),
        [ServerGamemode][ServerGamemode][ServerGamemode]nt.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
}

[ServerGamemode]oid ch[ServerGamemode]ckMatchWinConditions([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d, [ServerGamemode]int32_t tick,
                             [ServerGamemode]OCKET sock,
                             std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                             [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    if (d.matchMod[ServerGamemode] == "ffa") {
        fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : d.ffaKi[ServerGamemode][ServerGamemode]s) {
            if (k[ServerGamemode].s[ServerGamemode]cond >= d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode]) {
                d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
                d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode];
                d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = k[ServerGamemode].fi[ServerGamemode]st;
                d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 0;  // [ServerGamemode]co[ServerGamemode][ServerGamemode]Limit
                d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode](d, tick, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] FFA match o[ServerGamemode][ServerGamemode][ServerGamemode] winn[ServerGamemode][ServerGamemode]=%[ServerGamemode] sco[ServerGamemode][ServerGamemode]=%d goa[ServerGamemode]=%d\n",
                    k[ServerGamemode].fi[ServerGamemode]st, k[ServerGamemode].s[ServerGamemode]cond, d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode]);
                [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
            }
        }
    } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "tdm") {
        if (d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s >= d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] || d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s >= d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode]) {
            d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode];
            d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s >= d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s ? 0 : 1;
            d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 0;  // [ServerGamemode]co[ServerGamemode][ServerGamemode]Limit
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode](d, tick, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] T[ServerGamemode]M match o[ServerGamemode][ServerGamemode][ServerGamemode] winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am=%d [ServerGamemode][ServerGamemode]d=%d b[ServerGamemode][ServerGamemode][ServerGamemode]=%d goa[ServerGamemode]=%d\n",
                d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am, d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s, d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s, d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode]);
            [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
        }
    }

    // Tim[ServerGamemode] [ServerGamemode]imit ch[ServerGamemode]ck
    if (d.matchTim[ServerGamemode]LimitTick > 0 && tick >= d.matchTim[ServerGamemode]LimitTick) {
        d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
        d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode];
        d.[ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 1;  // Tim[ServerGamemode]Limit
        d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
        if (d.matchMod[ServerGamemode] == "ffa") {
            int b[ServerGamemode]st = -1;
            fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : d.ffaKi[ServerGamemode][ServerGamemode]s) {
                if (k[ServerGamemode].s[ServerGamemode]cond > b[ServerGamemode]st) {
                    b[ServerGamemode]st = k[ServerGamemode].s[ServerGamemode]cond;
                    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = k[ServerGamemode].fi[ServerGamemode]st;
                }
            }
            [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode](d, tick, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "tdm") {
            d.winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s >= d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s ? 0 : 1;
            [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode](d, tick, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
        }
        ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
        b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] Tim[ServerGamemode] [ServerGamemode]imit [ServerGamemode][ServerGamemode]ach[ServerGamemode]d mod[ServerGamemode]=%s [ServerGamemode][ServerGamemode]d=%d b[ServerGamemode][ServerGamemode][ServerGamemode]=%d\n",
            d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s, d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s);
    }
}

} // nam[ServerGamemode]spac[ServerGamemode]

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]Tick([ServerGamemode]OCKET sock,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                    H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                    Wo[ServerGamemode][ServerGamemode]d& npcWo[ServerGamemode][ServerGamemode]d,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                    Npc[ServerGamemode]yst[ServerGamemode]m& npc[ServerGamemode]yst[ServerGamemode]m,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_s[ServerGamemode]t<[ServerGamemode]int32_t>& npcIdsA[ServerGamemode]i[ServerGamemode][ServerGamemode],
                    [ServerGamemode]int32_t tick,
                    [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    d.c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = tick;
    if (d.mapOn[ServerGamemode]y)
    {
        const [ServerGamemode]int64_t now = nowMs();
        if (d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] && now >= d.comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs) {
            d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
            d.comm[ServerGamemode]nity[ServerGamemode]co[ServerGamemode][ServerGamemode]s.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
            d.comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][0] = d.comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][1] = 0;
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king, "[COMMUNITY MATCH] n[ServerGamemode]w [ServerGamemode]o[ServerGamemode]nd mod[ServerGamemode]=%s\n", d.comm[ServerGamemode]nityMod[ServerGamemode].c_st[ServerGamemode]());
        }
        if (d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode]) {
            const [ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] = d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id;
            d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
            if (!d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] && ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] != 0) {
                if (d.comm[ServerGamemode]nityMod[ServerGamemode] == "t[ServerGamemode]am_d[ServerGamemode]athmatch") {
                    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<[ServerGamemode]int32_t> ids;
                    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
                        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] == [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) ids.p[ServerGamemode]sh_back(k[ServerGamemode].fi[ServerGamemode]st);
                    std::so[ServerGamemode]t(ids.b[ServerGamemode]gin(), ids.[ServerGamemode]nd());
                    fo[ServerGamemode] (siz[ServerGamemode]_t i = 0; i < ids.siz[ServerGamemode](); ++i)
                        d.comm[ServerGamemode]nityT[ServerGamemode]ams[ids[i]] = (int)(i % 2);
                    const a[ServerGamemode]to t[ServerGamemode]amIt = d.comm[ServerGamemode]nityT[ServerGamemode]ams.find(ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]);
                    if (t[ServerGamemode]amIt != d.comm[ServerGamemode]nityT[ServerGamemode]ams.[ServerGamemode]nd()) {
                        const int t[ServerGamemode]am = t[ServerGamemode]amIt->s[ServerGamemode]cond;
                        if (++d.comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][t[ServerGamemode]am] >= 30) {
                            d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
                            const std::st[ServerGamemode]ing m[ServerGamemode]ssag[ServerGamemode] = "T[ServerGamemode]am " + std::to_st[ServerGamemode]ing(t[ServerGamemode]am + 1) +
                                " wins T[ServerGamemode]am [ServerGamemode][ServerGamemode]athmatch (30 ki[ServerGamemode][ServerGamemode]s)!";
                            b[ServerGamemode]oadcastComm[ServerGamemode]nityNotification(sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, m[ServerGamemode]ssag[ServerGamemode], 300, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                            d.comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs = now + 5000;
                        }
                    }
                } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.comm[ServerGamemode]nityMod[ServerGamemode] == "f[ServerGamemode][ServerGamemode][ServerGamemode]_fo[ServerGamemode]_a[ServerGamemode][ServerGamemode]") {
                    const int sco[ServerGamemode][ServerGamemode] = ++d.comm[ServerGamemode]nity[ServerGamemode]co[ServerGamemode][ServerGamemode]s[ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]];
                    if (sco[ServerGamemode][ServerGamemode] >= 20) {
                        d.comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
                        const a[ServerGamemode]to winn[ServerGamemode][ServerGamemode] = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]);
                        const std::st[ServerGamemode]ing nam[ServerGamemode] = winn[ServerGamemode][ServerGamemode] == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ? "P[ServerGamemode]ay[ServerGamemode][ServerGamemode]" : winn[ServerGamemode][ServerGamemode]->s[ServerGamemode]cond.nam[ServerGamemode];
                        const std::st[ServerGamemode]ing m[ServerGamemode]ssag[ServerGamemode] = nam[ServerGamemode] + " wins F[ServerGamemode][ServerGamemode][ServerGamemode] Fo[ServerGamemode] A[ServerGamemode][ServerGamemode] (20 ki[ServerGamemode][ServerGamemode]s)!";
                        b[ServerGamemode]oadcastComm[ServerGamemode]nityNotification(sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, m[ServerGamemode]ssag[ServerGamemode], 300, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                        d.comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs = now + 5000;
                    }
                }
            }
        }
        if (d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map && d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.[ServerGamemode]mpty()) {
            d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap = d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map;
            d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
            d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = fa[ServerGamemode]s[ServerGamemode];
            d.mapChang[ServerGamemode]Co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tMs = now;
        }
        if (d.a[ServerGamemode]toMapRotation && d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.[ServerGamemode]mpty() && now >= d.n[ServerGamemode]xtMapRotationMs) {
            std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing> candidat[ServerGamemode]s;
            fo[ServerGamemode] (const a[ServerGamemode]to& candidat[ServerGamemode] : d.mapPoo[ServerGamemode])
                if (candidat[ServerGamemode] != d.mapId && !d.[ServerGamemode]s[ServerGamemode]dMaps.co[ServerGamemode]nt(candidat[ServerGamemode])) candidat[ServerGamemode]s.p[ServerGamemode]sh_back(candidat[ServerGamemode]);
            if (candidat[ServerGamemode]s.[ServerGamemode]mpty()) {
                d.[ServerGamemode]s[ServerGamemode]dMaps.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                d.[ServerGamemode]s[ServerGamemode]dMaps.ins[ServerGamemode][ServerGamemode]t(d.mapId);
                fo[ServerGamemode] (const a[ServerGamemode]to& candidat[ServerGamemode] : d.mapPoo[ServerGamemode])
                    if (candidat[ServerGamemode] != d.mapId) candidat[ServerGamemode]s.p[ServerGamemode]sh_back(candidat[ServerGamemode]);
            }
            if (!candidat[ServerGamemode]s.[ServerGamemode]mpty()) {
                std::mt19937 [ServerGamemode]ng(std::[ServerGamemode]andom_d[ServerGamemode][ServerGamemode]ic[ServerGamemode]{}());
                std::sh[ServerGamemode]ff[ServerGamemode][ServerGamemode](candidat[ServerGamemode]s.b[ServerGamemode]gin(), candidat[ServerGamemode]s.[ServerGamemode]nd(), [ServerGamemode]ng);
                d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap = candidat[ServerGamemode]s.f[ServerGamemode]ont();
                d.mapChang[ServerGamemode]Co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tMs = now;
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
                    "[COMMUNITY MAP ROTATION] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt=%s n[ServerGamemode]xt=%s candidat[ServerGamemode]s=%z[ServerGamemode] ([ServerGamemode]andomiz[ServerGamemode]d)\n",
                    d.mapId.c_st[ServerGamemode](), d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.c_st[ServerGamemode](), candidat[ServerGamemode]s.siz[ServerGamemode]());
            } [ServerGamemode][ServerGamemode]s[ServerGamemode] {
                d.n[ServerGamemode]xtMapRotationMs = now + ([ServerGamemode]int64_t)d.mapRotationMin[ServerGamemode]t[ServerGamemode]s * 60000[ServerGamemode][ServerGamemode][ServerGamemode];
            }
        }
        if (!d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.[ServerGamemode]mpty()) {
            const [ServerGamemode]int64_t [ServerGamemode][ServerGamemode]aps[ServerGamemode]d = now - d.mapChang[ServerGamemode]Co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tMs;
            const [ServerGamemode]int64_t int[ServerGamemode][ServerGamemode][ServerGamemode]a[ServerGamemode] = 30000;
            const [ServerGamemode]int64_t [ServerGamemode][ServerGamemode]maining = [ServerGamemode][ServerGamemode]aps[ServerGamemode]d >= int[ServerGamemode][ServerGamemode][ServerGamemode]a[ServerGamemode] ? 0 : int[ServerGamemode][ServerGamemode][ServerGamemode]a[ServerGamemode] - [ServerGamemode][ServerGamemode]aps[ServerGamemode]d;
            static std::st[ServerGamemode]ing [ServerGamemode]astNotic[ServerGamemode]Map;
            static [ServerGamemode]int32_t [ServerGamemode]astNotic[ServerGamemode] = UINT32_MAX;
            if ([ServerGamemode]astNotic[ServerGamemode]Map != d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap) {
                [ServerGamemode]astNotic[ServerGamemode]Map = d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap;
                [ServerGamemode]astNotic[ServerGamemode] = UINT32_MAX;
            }
            const [ServerGamemode]int32_t s[ServerGamemode]conds = ([ServerGamemode]int32_t)(([ServerGamemode][ServerGamemode]maining + 999) / 1000);
            if ([ServerGamemode][ServerGamemode]maining > 0 && [ServerGamemode][ServerGamemode]maining <= int[ServerGamemode][ServerGamemode][ServerGamemode]a[ServerGamemode] &&
                (s[ServerGamemode]conds == 30 || s[ServerGamemode]conds == 5 || s[ServerGamemode]conds == 3 || s[ServerGamemode]conds == 2 || s[ServerGamemode]conds == 1) &&
                s[ServerGamemode]conds != [ServerGamemode]astNotic[ServerGamemode]) {
                [ServerGamemode]astNotic[ServerGamemode] = s[ServerGamemode]conds;
                std::st[ServerGamemode]ing msg = "[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] changing map to " + d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap +
                                  " in " + std::to_st[ServerGamemode]ing(s[ServerGamemode]conds) + " s[ServerGamemode]c...";
                b[ServerGamemode]oadcastComm[ServerGamemode]nityNotification(sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, msg, 180, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king, "[COMMUNITY MAP NOTICE] %s\n", msg.c_st[ServerGamemode]());
            }
            if ([ServerGamemode][ServerGamemode]maining == 0) {
                const std::st[ServerGamemode]ing n[ServerGamemode]xt = d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap;
                d.p[ServerGamemode]ndingA[ServerGamemode]tomaticMap.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                [ServerGamemode]astNotic[ServerGamemode]Map.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                [ServerGamemode]astNotic[ServerGamemode] = UINT32_MAX;
                if ([ServerGamemode][ServerGamemode][ServerGamemode]oadGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, n[ServerGamemode]xt, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)) {
                    d.n[ServerGamemode]xtMapRotationMs = now + ([ServerGamemode]int64_t)d.mapRotationMin[ServerGamemode]t[ServerGamemode]s * 60000[ServerGamemode][ServerGamemode][ServerGamemode];
                    npc[ServerGamemode]yst[ServerGamemode]m.d[ServerGamemode]st[ServerGamemode]oyA[ServerGamemode][ServerGamemode]();
                    siz[ServerGamemode]_t spawnInd[ServerGamemode]x = 0;
                    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : npcs) {
                        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc& npc = k[ServerGamemode].s[ServerGamemode]cond;
                        if (!wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty()) {
                            const a[ServerGamemode]to& sp = wo[ServerGamemode][ServerGamemode]d.spawnPoints[spawnInd[ServerGamemode]x % wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]()];
                            npc.pos = [ServerGamemode]ff[ServerGamemode]cti[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawn(sp.position);
                            npc.yaw = sp.yaw;
                            ++spawnInd[ServerGamemode]x;
                        }
                        npc.h[ServerGamemode]a[ServerGamemode]th = 100;
                        ++npc.t[ServerGamemode]ansfo[ServerGamemode]mEpoch;
                    }
                    npcIdsA[ServerGamemode]i[ServerGamemode][ServerGamemode].c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::N[ServerGamemode]two[ServerGamemode]king,
                        "[COMMUNITY MAP CHANGE] [ServerGamemode]oad[ServerGamemode]d=%s n[ServerGamemode]xtRotationMs=%[ServerGamemode][ServerGamemode][ServerGamemode]\n",
                        n[ServerGamemode]xt.c_st[ServerGamemode](), ([ServerGamemode]nsign[ServerGamemode]d [ServerGamemode]ong [ServerGamemode]ong)d.n[ServerGamemode]xtMapRotationMs);
                } [ServerGamemode][ServerGamemode]s[ServerGamemode] {
                    d.n[ServerGamemode]xtMapRotationMs = now + 5000;
                }
            }
        }
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    }
    ([ServerGamemode]oid)tick;

    // Host-on[ServerGamemode]y chang[ServerGamemode]map command: swap th[ServerGamemode] map [ServerGamemode]i[ServerGamemode][ServerGamemode] on th[ServerGamemode] n[ServerGamemode]xt tick.
    if (d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map)
    {
        const std::st[ServerGamemode]ing mapId = d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map;
        d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = fa[ServerGamemode]s[ServerGamemode];
        d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
        if (!mapId.[ServerGamemode]mpty())
        {
            if ([ServerGamemode][ServerGamemode][ServerGamemode]oadGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, mapId, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)) {
                npc[ServerGamemode]yst[ServerGamemode]m.d[ServerGamemode]st[ServerGamemode]oyA[ServerGamemode][ServerGamemode]();
                siz[ServerGamemode]_t spawnInd[ServerGamemode]x = 0;
                fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : npcs) {
                    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc& npc = k[ServerGamemode].s[ServerGamemode]cond;
                    if (!wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty()) {
                        const a[ServerGamemode]to& sp = wo[ServerGamemode][ServerGamemode]d.spawnPoints[spawnInd[ServerGamemode]x % wo[ServerGamemode][ServerGamemode]d.spawnPoints.siz[ServerGamemode]()];
                        npc.pos = [ServerGamemode]ff[ServerGamemode]cti[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawn(sp.position);
                        npc.yaw = sp.yaw;
                        ++spawnInd[ServerGamemode]x;
                    }
                    npc.h[ServerGamemode]a[ServerGamemode]th = 100;
                    ++npc.t[ServerGamemode]ansfo[ServerGamemode]mEpoch;
                }
                npcIdsA[ServerGamemode]i[ServerGamemode][ServerGamemode].c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
            }
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
    }

    // P[ServerGamemode]oc[ServerGamemode]ss any ki[ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]co[ServerGamemode]d[ServerGamemode]d by app[ServerGamemode]y[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]amag[ServerGamemode] [ServerGamemode]ast tick.
    if (d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode])
    {
        d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
        const [ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id;
        const [ServerGamemode]int32_t [ServerGamemode]ictimId = d.p[ServerGamemode]ndingVictimId;

        // Instant [ServerGamemode][ServerGamemode]spawn n[ServerGamemode]a[ServerGamemode] th[ServerGamemode] match ancho[ServerGamemode] with f[ServerGamemode][ServerGamemode][ServerGamemode] HP/ammo and a f[ServerGamemode][ServerGamemode]sh
        // [ServerGamemode]andom offs[ServerGamemode]t so th[ServerGamemode] [ServerGamemode]xact [ServerGamemode][ServerGamemode]spawn spot is n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] p[ServerGamemode][ServerGamemode]dictab[ServerGamemode][ServerGamemode].
        a[ServerGamemode]to [ServerGamemode]ictimIt = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find([ServerGamemode]ictimId);
        if ([ServerGamemode]ictimIt != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
        {
            [ServerGamemode]ictimIt->s[ServerGamemode]cond.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
            [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        }

        // T[ServerGamemode][ServerGamemode][ServerGamemode] th[ServerGamemode] ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] wh[ServerGamemode][ServerGamemode][ServerGamemode] th[ServerGamemode] [ServerGamemode]ictim [ServerGamemode][ServerGamemode]spawn[ServerGamemode]d (t[ServerGamemode]ac[ServerGamemode][ServerGamemode]).
        if (ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id != [ServerGamemode]ictimId)
        {
            [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]En[ServerGamemode]my[ServerGamemode]pawnPack[ServerGamemode]t t[ServerGamemode]ac[ServerGamemode][ServerGamemode]{};
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_[ServerGamemode]UEL_ENEMY_[ServerGamemode]PAWN;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].h[ServerGamemode]ad[ServerGamemode][ServerGamemode].tick = 0;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].[ServerGamemode]n[ServerGamemode]myP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = [ServerGamemode]ictimId;
            g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 spawnPos = [ServerGamemode]ictimIt != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() && [ServerGamemode]ictimIt->s[ServerGamemode]cond.has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos
                ? [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos : g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f);
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posX = spawnPos.x;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posY = spawnPos.y;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posZ = spawnPos.z;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].d[ServerGamemode][ServerGamemode][ServerGamemode]Id = d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].mapV[ServerGamemode][ServerGamemode]sion = d.mapV[ServerGamemode][ServerGamemode]sion;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = d.spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            t[ServerGamemode]ac[ServerGamemode][ServerGamemode].[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode] = ++d.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode];
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;

            a[ServerGamemode]to ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id);
            if (ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() && ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] == [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            {
                const [ServerGamemode]int32_t [ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
                const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
                    sock, ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond, &t[ServerGamemode]ac[ServerGamemode][ServerGamemode], siz[ServerGamemode]of(t[ServerGamemode]ac[ServerGamemode][ServerGamemode]), [ServerGamemode][ServerGamemode][ServerGamemode]ntId,
                    [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                const boo[ServerGamemode] s[ServerGamemode]nt = [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t == R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t::Q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d;
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Pack[ServerGamemode]t[ServerGamemode][ServerGamemode]nd] typ[ServerGamemode]=[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]En[ServerGamemode]my[ServerGamemode]pawnPack[ServerGamemode]t [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]=1 p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] [ServerGamemode]n[ServerGamemode]my=%[ServerGamemode] s[ServerGamemode]nt=%d pos=(%.3f,%.3f,%.3f)\n",
                    ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.id, [ServerGamemode]ictimId, (int)s[ServerGamemode]nt, t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posX, t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posY, t[ServerGamemode]ac[ServerGamemode][ServerGamemode].posZ);
                if (s[ServerGamemode]nt)
                    ++tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t;
            }
        }

        // [ServerGamemode]co[ServerGamemode][ServerGamemode] on[ServerGamemode]y co[ServerGamemode]nts d[ServerGamemode][ServerGamemode]ing th[ServerGamemode] acti[ServerGamemode][ServerGamemode] phas[ServerGamemode], and n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] fo[ServerGamemode] a s[ServerGamemode]icid[ServerGamemode].
        if (d.phas[ServerGamemode] == [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE && !d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] && ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id != [ServerGamemode]ictimId)
        {
            // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] 1[ServerGamemode]1 sco[ServerGamemode]ing (o[ServerGamemode]igina[ServerGamemode] b[ServerGamemode]ha[ServerGamemode]io[ServerGamemode])
            if (d.matchMod[ServerGamemode] == "d[ServerGamemode][ServerGamemode][ServerGamemode]") {
                if (ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id == d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId)
                    ++d.sco[ServerGamemode][ServerGamemode]A;
                [ServerGamemode][ServerGamemode]s[ServerGamemode] if (ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id == d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId)
                    ++d.sco[ServerGamemode][ServerGamemode]B;

                if (d.sco[ServerGamemode][ServerGamemode]A >= d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] || d.sco[ServerGamemode][ServerGamemode]B >= d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode])
                {
                    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
                    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_MATCH_EN[ServerGamemode];
                    d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id;
                    d.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft = d.[ServerGamemode][ServerGamemode]match[ServerGamemode][ServerGamemode]conds;
                    [ServerGamemode]mitGam[ServerGamemode]mod[ServerGamemode]MatchP[ServerGamemode][ServerGamemode]sist[ServerGamemode]nc[ServerGamemode](d, tick, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
                    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                        "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] match o[ServerGamemode][ServerGamemode][ServerGamemode] winn[ServerGamemode][ServerGamemode]=%[ServerGamemode] sco[ServerGamemode][ServerGamemode]=%d-%d goa[ServerGamemode]=%d\n",
                        d.winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, d.sco[ServerGamemode][ServerGamemode]A, d.sco[ServerGamemode][ServerGamemode]B, d.goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode]);
                }
            }
            // FFA sco[ServerGamemode]ing
            [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "ffa") {
                ++d.ffaKi[ServerGamemode][ServerGamemode]s[ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id];
                ++d.ffa[ServerGamemode][ServerGamemode]aths[[ServerGamemode]ictimId];
                // Win condition ch[ServerGamemode]ck[ServerGamemode]d in ch[ServerGamemode]ckMatchWinConditions
            }
            // T[ServerGamemode]M sco[ServerGamemode]ing
            [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.matchMod[ServerGamemode] == "tdm") {
                ++d.ffaKi[ServerGamemode][ServerGamemode]s[ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id];
                ++d.ffa[ServerGamemode][ServerGamemode]aths[[ServerGamemode]ictimId];
                a[ServerGamemode]to t[ServerGamemode]amIt = d.matchT[ServerGamemode]ams.find(ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id);
                if (t[ServerGamemode]amIt != d.matchT[ServerGamemode]ams.[ServerGamemode]nd()) {
                    int [ServerGamemode]ictimT[ServerGamemode]am = -1;
                    a[ServerGamemode]to [ServerGamemode]tIt = d.matchT[ServerGamemode]ams.find([ServerGamemode]ictimId);
                    if ([ServerGamemode]tIt != d.matchT[ServerGamemode]ams.[ServerGamemode]nd()) [ServerGamemode]ictimT[ServerGamemode]am = [ServerGamemode]tIt->s[ServerGamemode]cond;
                    // On[ServerGamemode]y sco[ServerGamemode][ServerGamemode] if ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] and [ServerGamemode]ictim a[ServerGamemode][ServerGamemode] on diff[ServerGamemode][ServerGamemode][ServerGamemode]nt t[ServerGamemode]ams
                    if ([ServerGamemode]ictimT[ServerGamemode]am >= 0 && t[ServerGamemode]amIt->s[ServerGamemode]cond != [ServerGamemode]ictimT[ServerGamemode]am) {
                        if (t[ServerGamemode]amIt->s[ServerGamemode]cond == 0) ++d.[ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
                        [ServerGamemode][ServerGamemode]s[ServerGamemode] ++d.b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s;
                    }
                }
            }
        }

        b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
    }

    // ── FFA/T[ServerGamemode]M match mod[ServerGamemode] stat[ServerGamemode] machin[ServerGamemode] ────────────────────────────
    if (d.matchMod[ServerGamemode] == "ffa" || d.matchMod[ServerGamemode] == "tdm")
    {
        if (d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding)
        {
            // Th[ServerGamemode] command chang[ServerGamemode]d th[ServerGamemode] a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] mod[ServerGamemode]/phas[ServerGamemode] b[ServerGamemode]tw[ServerGamemode][ServerGamemode]n ticks.
            // [ServerGamemode][ServerGamemode]nd that stat[ServerGamemode] now so [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y c[ServerGamemode]i[ServerGamemode]nt can show int[ServerGamemode][ServerGamemode]mission o[ServerGamemode] th[ServerGamemode]
            // imm[ServerGamemode]diat[ServerGamemode] co[ServerGamemode]ntdown witho[ServerGamemode]t waiting fo[ServerGamemode] th[ServerGamemode] p[ServerGamemode][ServerGamemode]iodic b[ServerGamemode]oadcast.
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = fa[ServerGamemode]s[ServerGamemode];
        }
        switch (d.phas[ServerGamemode])
        {
        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING:
            if (co[ServerGamemode]ntActi[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]s(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) >= 2 ||
                (co[ServerGamemode]ntActi[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]s(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) >= 1 && !npcs.[ServerGamemode]mpty()))
            {
                // If th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt map has no spawn points, [ServerGamemode]otat[ServerGamemode].
                if (wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty())
                    [ServerGamemode]otat[ServerGamemode]ToN[ServerGamemode]xtGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
                assignMatchPa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, &npcs);
                b[ServerGamemode]ginMatchCo[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tick);
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[FFA/T[ServerGamemode]M] Co[ServerGamemode]ntdown sta[ServerGamemode]t[ServerGamemode]d mod[ServerGamemode]=%s pa[ServerGamemode]ticipants=%z[ServerGamemode]\n",
                    d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
            }
            b[ServerGamemode][ServerGamemode]ak;

        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_COUNT[ServerGamemode]OWN:
            if (tick >= d.match[ServerGamemode]ta[ServerGamemode]tTick)
            {
                d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_GO;
                d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.go[ServerGamemode][ServerGamemode]conds;
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[FFA/T[ServerGamemode]M] GO shown mod[ServerGamemode]=%s tick=%[ServerGamemode] d[ServerGamemode][ServerGamemode]ation=%.2f\n",
                    d.matchMod[ServerGamemode].c_st[ServerGamemode](), tick, d.go[ServerGamemode][ServerGamemode]conds);
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
            {
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            b[ServerGamemode][ServerGamemode]ak;

        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_GO:
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
            if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f)
            {
                d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE;
                d.match[ServerGamemode]ta[ServerGamemode]tTick = tick;
                if (d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds > 0)
                    d.matchTim[ServerGamemode]LimitTick = tick + ([ServerGamemode]int32_t)(d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds * 60.0f);
                [ServerGamemode][ServerGamemode]spawnA[ServerGamemode][ServerGamemode]Pa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[FFA/T[ServerGamemode]M] Match ACTIVE mod[ServerGamemode]=%s tick=%[ServerGamemode]\n",
                    d.matchMod[ServerGamemode].c_st[ServerGamemode](), tick, d.match[ServerGamemode]ta[ServerGamemode]tTick);
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            b[ServerGamemode][ServerGamemode]ak;

        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE:
            // Ch[ServerGamemode]ck win conditions on [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y tick
            ch[ServerGamemode]ckMatchWinConditions(d, tick, sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            if (d.phas[ServerGamemode] != [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE) b[ServerGamemode][ServerGamemode]ak;  // win condition t[ServerGamemode]igg[ServerGamemode][ServerGamemode][ServerGamemode]d
            // P[ServerGamemode][ServerGamemode]iodic b[ServerGamemode]oadcast
            if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
            {
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            b[ServerGamemode][ServerGamemode]ak;

        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode]:
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
            if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f)
            {
                if (d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witch && !d.p[ServerGamemode]ndingGam[ServerGamemode]mod[ServerGamemode]Id.[ServerGamemode]mpty())
                {
                    const std::st[ServerGamemode]ing n[ServerGamemode]xtMod[ServerGamemode] = d.p[ServerGamemode]ndingGam[ServerGamemode]mod[ServerGamemode]Id;
                    const boo[ServerGamemode] di[ServerGamemode][ServerGamemode]ctCo[ServerGamemode]ntdown = d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witchCo[ServerGamemode]ntdown;
                    d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witch = fa[ServerGamemode]s[ServerGamemode];
                    d.p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witchCo[ServerGamemode]ntdown = fa[ServerGamemode]s[ServerGamemode];
                    d.p[ServerGamemode]ndingGam[ServerGamemode]mod[ServerGamemode]Id.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING;
                    d.matchMod[ServerGamemode].c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
                    s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode]ta[ServerGamemode]tMatch(di[ServerGamemode][ServerGamemode]ctCo[ServerGamemode]ntdown, n[ServerGamemode]xtMod[ServerGamemode]);
                    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
                }
                d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION;
                d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[FFA/T[ServerGamemode]M] Int[ServerGamemode][ServerGamemode]mission sta[ServerGamemode]t[ServerGamemode]d mod[ServerGamemode]=%s d[ServerGamemode][ServerGamemode]ation=%.0f\n",
                    d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds);
            }
            [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
            {
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            b[ServerGamemode][ServerGamemode]ak;

        cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION:
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
            if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f)
            {
                // Rotat[ServerGamemode] map if config[ServerGamemode][ServerGamemode][ServerGamemode]d
                if (d.[ServerGamemode]otat[ServerGamemode]Maps && d.mapPoo[ServerGamemode].siz[ServerGamemode]() > 1)
                    [ServerGamemode]otat[ServerGamemode]ToN[ServerGamemode]xtGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
                assignMatchPa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, &npcs);
                b[ServerGamemode]ginMatchCo[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tick);
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[FFA/T[ServerGamemode]M] Co[ServerGamemode]ntdown sta[ServerGamemode]t[ServerGamemode]d mod[ServerGamemode]=%s pa[ServerGamemode]ticipants=%z[ServerGamemode]\n",
                    d.matchMod[ServerGamemode].c_st[ServerGamemode](), d.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
            }
            [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
            {
                d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
                b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
            b[ServerGamemode][ServerGamemode]ak;

        d[ServerGamemode]fa[ServerGamemode][ServerGamemode]t:
            b[ServerGamemode][ServerGamemode]ak;
        }
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    }

    // ── Bomb Tag match mod[ServerGamemode] stat[ServerGamemode] machin[ServerGamemode] ──────────────────────────
    if (d.hasBombF[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode])
    {
        if (d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding)
        {
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = fa[ServerGamemode]s[ServerGamemode];
        }
        s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTagTick(sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcs, npc[ServerGamemode]yst[ServerGamemode]m, tick, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    }

    // ── O[ServerGamemode]igina[ServerGamemode] d[ServerGamemode][ServerGamemode][ServerGamemode] 1[ServerGamemode]1 stat[ServerGamemode] machin[ServerGamemode] ─────────────────────────────
    switch (d.phas[ServerGamemode])
    {
    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING:
        if (co[ServerGamemode]ntActi[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]s(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) >= 2)
        {
            assignGam[ServerGamemode]mod[ServerGamemode]Pa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            // If th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt map has no spawn points ([ServerGamemode].g. th[ServerGamemode] host pick[ServerGamemode]d a
            // spawn-[ServerGamemode][ServerGamemode]ss map), [ServerGamemode]otat[ServerGamemode] to a spawn-capab[ServerGamemode][ServerGamemode] on[ServerGamemode] b[ServerGamemode]fo[ServerGamemode][ServerGamemode] sta[ServerGamemode]ting.
            if (wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty())
                [ServerGamemode]otat[ServerGamemode]ToN[ServerGamemode]xtGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            // [ServerGamemode][ServerGamemode]op th[ServerGamemode] p[ServerGamemode]actic[ServerGamemode] NPC(s) onc[ServerGamemode] th[ServerGamemode] [ServerGamemode][ServerGamemode]a[ServerGamemode] d[ServerGamemode][ServerGamemode][ServerGamemode] is abo[ServerGamemode]t to sta[ServerGamemode]t.
            npcs.c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
            npc[ServerGamemode]yst[ServerGamemode]m.d[ServerGamemode]st[ServerGamemode]oyA[ServerGamemode][ServerGamemode]();
            npcIdsA[ServerGamemode]i[ServerGamemode][ServerGamemode].c[ServerGamemode][ServerGamemode]a[ServerGamemode]();
            assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
            b[ServerGamemode]ginGam[ServerGamemode]mod[ServerGamemode]Co[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_COUNT[ServerGamemode]OWN:
        // A d[ServerGamemode][ServerGamemode][ServerGamemode]ist [ServerGamemode]anish[ServerGamemode]d b[ServerGamemode]fo[ServerGamemode][ServerGamemode] th[ServerGamemode] fight — fa[ServerGamemode][ServerGamemode] back to waiting.
        if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ||
            p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
        {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING;
            d.stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
            b[ServerGamemode][ServerGamemode]ak;
        }
        d.co[ServerGamemode]ntdown -= [ServerGamemode]ERVER_[ServerGamemode]T;
        if (d.co[ServerGamemode]ntdown <= 0.0f)
        {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE;
            d.stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]] co[ServerGamemode]ntdown comp[ServerGamemode][ServerGamemode]t[ServerGamemode] d[ServerGamemode][ServerGamemode][ServerGamemode]Id=%[ServerGamemode] stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion=%[ServerGamemode] phas[ServerGamemode]=ACTIVE\n",
                d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id, d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion);
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        // [ServerGamemode][ServerGamemode][ServerGamemode]ing co[ServerGamemode]ntdown, do NOT b[ServerGamemode]oadcast [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y tick. R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode] d[ServerGamemode][ServerGamemode]i[ServerGamemode][ServerGamemode][ServerGamemode]y of
        // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y co[ServerGamemode]ntdown snapshot c[ServerGamemode][ServerGamemode]at[ServerGamemode]s a h[ServerGamemode]g[ServerGamemode] back[ServerGamemode]og that b[ServerGamemode]ocks th[ServerGamemode]
        // ACTIVE t[ServerGamemode]ansition f[ServerGamemode]om b[ServerGamemode]ing d[ServerGamemode][ServerGamemode]i[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d p[ServerGamemode]ompt[ServerGamemode]y. Th[ServerGamemode] initia[ServerGamemode]
        // co[ServerGamemode]ntdown sta[ServerGamemode]t is b[ServerGamemode]oadcast by b[ServerGamemode]ginGam[ServerGamemode]mod[ServerGamemode]Co[ServerGamemode]ntdown(); th[ServerGamemode] ACTIVE
        // t[ServerGamemode]ansition is b[ServerGamemode]oadcast abo[ServerGamemode][ServerGamemode]. C[ServerGamemode]i[ServerGamemode]nts int[ServerGamemode][ServerGamemode]po[ServerGamemode]at[ServerGamemode] co[ServerGamemode]ntdown [ServerGamemode]oca[ServerGamemode][ServerGamemode]y.
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE:
        if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ||
            p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
        {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING;
            d.stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
        {
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_MATCH_EN[ServerGamemode]:
        if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() ||
            p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId) == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd())
        {
            // Oppon[ServerGamemode]nt [ServerGamemode][ServerGamemode]ft d[ServerGamemode][ServerGamemode]ing th[ServerGamemode] [ServerGamemode]nd sc[ServerGamemode][ServerGamemode][ServerGamemode]n — no [ServerGamemode][ServerGamemode]match.
            d.stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode][ServerGamemode]ak;
        }
        d.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft -= [ServerGamemode]ERVER_[ServerGamemode]T;
        if (d.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft <= 0.0f)
        {
            // Rotat[ServerGamemode] to a f[ServerGamemode][ServerGamemode]sh map w[ServerGamemode] w[ServerGamemode][ServerGamemode][ServerGamemode]n't j[ServerGamemode]st on (skips bad/spawn-[ServerGamemode][ServerGamemode]ss maps).
            if (d.[ServerGamemode]otat[ServerGamemode]Maps && d.mapPoo[ServerGamemode].siz[ServerGamemode]() > 1)
                [ServerGamemode]otat[ServerGamemode]ToN[ServerGamemode]xtGam[ServerGamemode]mod[ServerGamemode]Map(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, wo[ServerGamemode][ServerGamemode]d, npcWo[ServerGamemode][ServerGamemode]d, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            // Each n[ServerGamemode]w match picks a f[ServerGamemode][ServerGamemode]sh [ServerGamemode]andom ancho[ServerGamemode] (fights sp[ServerGamemode][ServerGamemode]ad a[ServerGamemode]o[ServerGamemode]nd).
            assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode], "[GAMEMO[ServerGamemode]E [ServerGamemode]ERVER] [ServerGamemode][ServerGamemode]match\n");
            b[ServerGamemode]ginGam[ServerGamemode]mod[ServerGamemode]Co[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60)
        {
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    d[ServerGamemode]fa[ServerGamemode][ServerGamemode]t:
        b[ServerGamemode][ServerGamemode]ak;
    }
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]matchNow()
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    // Th[ServerGamemode] MATCH_EN[ServerGamemode] b[ServerGamemode]anch sta[ServerGamemode]ts th[ServerGamemode] n[ServerGamemode]xt d[ServerGamemode][ServerGamemode][ServerGamemode] wh[ServerGamemode]n [ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft hits 0.
    d.[ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft = 0.0f;
}

std::st[ServerGamemode]ing s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Acti[ServerGamemode][ServerGamemode]T[ServerGamemode]amList()
{
    const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config& config = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]();
    if (config.mod[ServerGamemode]s().[ServerGamemode]mpty()) config.[ServerGamemode]oad();
    const Comm[ServerGamemode]nityMod[ServerGamemode]* cm = config.mod[ServerGamemode]ById(d.comm[ServerGamemode]nityMod[ServerGamemode]);
    const std::st[ServerGamemode]ing id = cm ? cm->gam[ServerGamemode]mod[ServerGamemode]Id : d.comm[ServerGamemode]nityMod[ServerGamemode];
    const Gam[ServerGamemode]mod[ServerGamemode]& gm = Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]gist[ServerGamemode]y::instanc[ServerGamemode]().g[ServerGamemode]t(id);
    if (gm.t[ServerGamemode]amNam[ServerGamemode]s.[ServerGamemode]mpty()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n "no t[ServerGamemode]ams";
    std::st[ServerGamemode]ing o[ServerGamemode]t;
    fo[ServerGamemode] (siz[ServerGamemode]_t i = 0; i < gm.t[ServerGamemode]amNam[ServerGamemode]s.siz[ServerGamemode](); ++i) {
        if (!o[ServerGamemode]t.[ServerGamemode]mpty()) o[ServerGamemode]t += " | ";
        o[ServerGamemode]t += gm.t[ServerGamemode]amNam[ServerGamemode]s[i] + " = " + std::to_st[ServerGamemode]ing(i + 1);
    }

    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n o[ServerGamemode]t;
}

boo[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]q[ServerGamemode][ServerGamemode]stT[ServerGamemode]amChang[ServerGamemode]([ServerGamemode]int32_t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, int [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am,
                             [ServerGamemode]OCKET sock,
                             std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                             [ServerGamemode]int32_t tick, [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t,
                             std::st[ServerGamemode]ing& m[ServerGamemode]ssag[ServerGamemode])
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    a[ServerGamemode]to p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
    if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) { m[ServerGamemode]ssag[ServerGamemode] = "p[ServerGamemode]ay[ServerGamemode][ServerGamemode] not fo[ServerGamemode]nd"; [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode]; }
    const Comm[ServerGamemode]nityMod[ServerGamemode]* cm = Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Config::instanc[ServerGamemode]().mod[ServerGamemode]ById(d.comm[ServerGamemode]nityMod[ServerGamemode]);
    const std::st[ServerGamemode]ing id = cm ? cm->gam[ServerGamemode]mod[ServerGamemode]Id : d.comm[ServerGamemode]nityMod[ServerGamemode];
    const Gam[ServerGamemode]mod[ServerGamemode]& gm = Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]gist[ServerGamemode]y::instanc[ServerGamemode]().g[ServerGamemode]t(id);
    if ([ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am < 0 || [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am >= static_cast<int>(gm.t[ServerGamemode]amNam[ServerGamemode]s.siz[ServerGamemode]())) {
        m[ServerGamemode]ssag[ServerGamemode] = gm.t[ServerGamemode]amNam[ServerGamemode]s.[ServerGamemode]mpty() ? "acti[ServerGamemode][ServerGamemode] gam[ServerGamemode]mod[ServerGamemode] has no t[ServerGamemode]ams" : "in[ServerGamemode]a[ServerGamemode]id t[ServerGamemode]am";
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n fa[ServerGamemode]s[ServerGamemode];
    }
    p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.matchT[ServerGamemode]am = [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am;
    d.matchT[ServerGamemode]ams[p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id] = [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am;
    m[ServerGamemode]ssag[ServerGamemode] = "switch[ServerGamemode]d to " + gm.t[ServerGamemode]amNam[ServerGamemode]s[static_cast<siz[ServerGamemode]_t>([ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am)];
    b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ChatM[ServerGamemode]ssag[ServerGamemode](sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tick, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t,
        (p[ServerGamemode]ay[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.nam[ServerGamemode] + " " + m[ServerGamemode]ssag[ServerGamemode]).c_st[ServerGamemode]());
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[MATCH TEAM] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] t[ServerGamemode]am=%d mod[ServerGamemode]=%s [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t=acc[ServerGamemode]pt[ServerGamemode]d\n",
        p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am, id.c_st[ServerGamemode]());
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n t[ServerGamemode][ServerGamemode][ServerGamemode];
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]spawnA[ServerGamemode][ServerGamemode]Acto[ServerGamemode]s([ServerGamemode]OCKET sock,
                            std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                            std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                            [ServerGamemode]int32_t tick, [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p[ServerGamemode]ay[ServerGamemode][ServerGamemode] = k[ServerGamemode].s[ServerGamemode]cond;
        if (p[ServerGamemode]ay[ServerGamemode][ServerGamemode].spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) contin[ServerGamemode][ServerGamemode];
        p[ServerGamemode]ay[ServerGamemode][ServerGamemode].d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        p[ServerGamemode]ay[ServerGamemode][ServerGamemode].has[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = t[ServerGamemode][ServerGamemode][ServerGamemode];
        p[ServerGamemode]ay[ServerGamemode][ServerGamemode].pos = p[ServerGamemode]ay[ServerGamemode][ServerGamemode].d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos;
        comp[ServerGamemode][ServerGamemode]t[ServerGamemode]A[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode][ServerGamemode]pawn(sock, p[ServerGamemode]ay[ServerGamemode][ServerGamemode], fa[ServerGamemode]s[ServerGamemode]);
    }
    fo[ServerGamemode] (a[ServerGamemode]to& k[ServerGamemode] : npcs) {
        [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc& npc = k[ServerGamemode].s[ServerGamemode]cond;
        npc.h[ServerGamemode]a[ServerGamemode]th = 100;
        npc.knockbackImp[ServerGamemode][ServerGamemode]s[ServerGamemode] = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f);
        ++npc.t[ServerGamemode]ansfo[ServerGamemode]mEpoch;
    }
    d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = 0;
    d.p[ServerGamemode]ndingVictimId = 0;
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]IsNpc = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[MATCH RE[ServerGamemode]PAWN ALL] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s=%z[ServerGamemode] npcs=%z[ServerGamemode] tick=%[ServerGamemode]\n",
        p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.siz[ServerGamemode](), npcs.siz[ServerGamemode](), tick);
    b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]q[ServerGamemode][ServerGamemode]stMapChang[ServerGamemode](const std::st[ServerGamemode]ing& mapId)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d || mapId.[ServerGamemode]mpty()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    d.hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = mapId;
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]OnP[ServerGamemode]ay[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ath([ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id,
                             [ServerGamemode]int32_t [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    d.p[ServerGamemode]ndingVictimId = [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]IsNpc = fa[ServerGamemode]s[ServerGamemode];
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]OnNpc[ServerGamemode][ServerGamemode]ath([ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]NpcId,
                          [ServerGamemode]int32_t [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]NpcId;
    d.p[ServerGamemode]ndingVictimId = [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]IsNpc = t[ServerGamemode][ServerGamemode][ServerGamemode];
}

// ── Bomb Tag ──────────────────────────────────────────────────────────

nam[ServerGamemode]spac[ServerGamemode] {

// [ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode] bag fo[ServerGamemode] bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode]ction. Ens[ServerGamemode][ServerGamemode][ServerGamemode]s [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]
// is s[ServerGamemode][ServerGamemode][ServerGamemode]ct[ServerGamemode]d onc[ServerGamemode] b[ServerGamemode]fo[ServerGamemode][ServerGamemode] th[ServerGamemode] bag is [ServerGamemode][ServerGamemode]sh[ServerGamemode]ff[ServerGamemode][ServerGamemode]d.
st[ServerGamemode][ServerGamemode]ct Bomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag {
    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<[ServerGamemode]int32_t> o[ServerGamemode]d[ServerGamemode][ServerGamemode];
    int position = 0;

    [ServerGamemode]oid b[ServerGamemode]i[ServerGamemode]d(const std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<[ServerGamemode]int32_t>& [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode]) {
        o[ServerGamemode]d[ServerGamemode][ServerGamemode] = [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode];
        // Fish[ServerGamemode][ServerGamemode]-Yat[ServerGamemode]s sh[ServerGamemode]ff[ServerGamemode][ServerGamemode]
        fo[ServerGamemode] (int i = (int)o[ServerGamemode]d[ServerGamemode][ServerGamemode].siz[ServerGamemode]() - 1; i > 0; --i) {
            int j = (int)(([ServerGamemode]int32_t)std::[ServerGamemode]and() % ([ServerGamemode]int32_t)(i + 1));
            std::swap(o[ServerGamemode]d[ServerGamemode][ServerGamemode][i], o[ServerGamemode]d[ServerGamemode][ServerGamemode][j]);
        }
        position = 0;
    }

    [ServerGamemode]int32_t n[ServerGamemode]xt() {
        if (o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode]mpty()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n 0;
        if (position >= (int)o[ServerGamemode]d[ServerGamemode][ServerGamemode].siz[ServerGamemode]()) {
            // Bag [ServerGamemode]xha[ServerGamemode]st[ServerGamemode]d — [ServerGamemode][ServerGamemode]b[ServerGamemode]i[ServerGamemode]d with c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode] [ServerGamemode]ist
            // Ca[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] m[ServerGamemode]st ca[ServerGamemode][ServerGamemode] b[ServerGamemode]i[ServerGamemode]d() b[ServerGamemode]fo[ServerGamemode][ServerGamemode] n[ServerGamemode]xt() if th[ServerGamemode]y want a f[ServerGamemode][ServerGamemode]sh bag.
            // If ca[ServerGamemode][ServerGamemode][ServerGamemode]d witho[ServerGamemode]t b[ServerGamemode]i[ServerGamemode]d(), j[ServerGamemode]st w[ServerGamemode]ap a[ServerGamemode]o[ServerGamemode]nd.
            position = 0;
        }
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n o[ServerGamemode]d[ServerGamemode][ServerGamemode][position++];
    }

    [ServerGamemode]oid [ServerGamemode][ServerGamemode]mo[ServerGamemode][ServerGamemode]([ServerGamemode]int32_t id) {
        a[ServerGamemode]to it = std::find(o[ServerGamemode]d[ServerGamemode][ServerGamemode].b[ServerGamemode]gin() + position, o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode]nd(), id);
        if (it != o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode]nd()) {
            o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode][ServerGamemode]as[ServerGamemode](it);
            if (position >= (int)o[ServerGamemode]d[ServerGamemode][ServerGamemode].siz[ServerGamemode]() && !o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode]mpty())
                position = 0;
        }
    }
};

Bomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag sBomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag;

// [ServerGamemode]imp[ServerGamemode][ServerGamemode] sph[ServerGamemode][ServerGamemode][ServerGamemode] o[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ap t[ServerGamemode]st fo[ServerGamemode] bomb contact d[ServerGamemode]t[ServerGamemode]ction on th[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].
// Us[ServerGamemode]s p[ServerGamemode]ay[ServerGamemode][ServerGamemode] body-pa[ServerGamemode]t sph[ServerGamemode][ServerGamemode][ServerGamemode] positions if a[ServerGamemode]ai[ServerGamemode]ab[ServerGamemode][ServerGamemode], oth[ServerGamemode][ServerGamemode]wis[ServerGamemode] [ServerGamemode]oot position.
boo[ServerGamemode] bomb[ServerGamemode]ph[ServerGamemode][ServerGamemode][ServerGamemode]O[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ap(const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3& aPos, f[ServerGamemode]oat aRadi[ServerGamemode]s,
                       const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3& bPos, f[ServerGamemode]oat bRadi[ServerGamemode]s)
{
    f[ServerGamemode]oat dist = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]ngth(aPos - bPos);
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n dist < (aRadi[ServerGamemode]s + bRadi[ServerGamemode]s);
}

// G[ServerGamemode]t a [ServerGamemode]o[ServerGamemode]gh position fo[ServerGamemode] an [ServerGamemode]ntity ([ServerGamemode]oot position fo[ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, body.pos fo[ServerGamemode] NPCs).
g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 g[ServerGamemode]tEntityRootPos(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]& p) {
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n p.pos;
}

g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 g[ServerGamemode]tEntityRootPos(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc& n) {
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n n.pos;
}

// Find th[ServerGamemode] bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode]'s wo[ServerGamemode][ServerGamemode]d position f[ServerGamemode]om s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] stat[ServerGamemode].
g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 g[ServerGamemode]tBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode]Position(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                                const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                                const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs)
{
    if (d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] == 1 /* p[ServerGamemode]ay[ServerGamemode][ServerGamemode] */) {
        a[ServerGamemode]to it = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
        if (it != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n g[ServerGamemode]tEntityRootPos(it->s[ServerGamemode]cond);
    } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] == 2 /* npc */) {
        a[ServerGamemode]to it = npcs.find(([ServerGamemode]int32_t)d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x);
        if (it != npcs.[ServerGamemode]nd()) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n g[ServerGamemode]tEntityRootPos(it->s[ServerGamemode]cond);
    }
    [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f);
}

// [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ct a n[ServerGamemode]w bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode] [ServerGamemode]sing th[ServerGamemode] sh[ServerGamemode]ff[ServerGamemode][ServerGamemode] bag.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode]ctN[ServerGamemode]wBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode]([ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                         const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
{
    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<[ServerGamemode]int32_t> [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode];
    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] == [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode] && !k[ServerGamemode].s[ServerGamemode]cond.d[ServerGamemode]ad)
            [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode].p[ServerGamemode]sh_back(k[ServerGamemode].fi[ServerGamemode]st);
    }
    if ([ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode].[ServerGamemode]mpty()) {
        d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 0;
        d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
        d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = 0;
        [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    }
    // Ch[ServerGamemode]ck if c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt bag is [ServerGamemode]a[ServerGamemode]id fo[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode] s[ServerGamemode]t
    boo[ServerGamemode] n[ServerGamemode][ServerGamemode]dR[ServerGamemode]b[ServerGamemode]i[ServerGamemode]d = sBomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag.o[ServerGamemode]d[ServerGamemode][ServerGamemode].[ServerGamemode]mpty();
    if (!n[ServerGamemode][ServerGamemode]dR[ServerGamemode]b[ServerGamemode]i[ServerGamemode]d) {
        // Ch[ServerGamemode]ck if a[ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]maining bag [ServerGamemode]nt[ServerGamemode]i[ServerGamemode]s a[ServerGamemode][ServerGamemode] sti[ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode]
        fo[ServerGamemode] ([ServerGamemode]int32_t id : sBomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag.o[ServerGamemode]d[ServerGamemode][ServerGamemode]) {
            boo[ServerGamemode] fo[ServerGamemode]nd = fa[ServerGamemode]s[ServerGamemode];
            fo[ServerGamemode] ([ServerGamemode]int32_t [ServerGamemode] : [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode]) {
                if ([ServerGamemode] == id) { fo[ServerGamemode]nd = t[ServerGamemode][ServerGamemode][ServerGamemode]; b[ServerGamemode][ServerGamemode]ak; }
            }
            if (!fo[ServerGamemode]nd) { n[ServerGamemode][ServerGamemode]dR[ServerGamemode]b[ServerGamemode]i[ServerGamemode]d = t[ServerGamemode][ServerGamemode][ServerGamemode]; b[ServerGamemode][ServerGamemode]ak; }
        }
    }
    if (n[ServerGamemode][ServerGamemode]dR[ServerGamemode]b[ServerGamemode]i[ServerGamemode]d) {
        sBomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag.b[ServerGamemode]i[ServerGamemode]d([ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode]);
    }
    [ServerGamemode]int32_t chos[ServerGamemode]n = sBomb[ServerGamemode]h[ServerGamemode]ff[ServerGamemode][ServerGamemode]Bag.n[ServerGamemode]xt();
    if (chos[ServerGamemode]n == 0) {
        // Fa[ServerGamemode][ServerGamemode]back: [ServerGamemode]andom pick
        chos[ServerGamemode]n = [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode][([ServerGamemode]int32_t)std::[ServerGamemode]and() % [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode].siz[ServerGamemode]()];
    }
    d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 1;
    d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = chos[ServerGamemode]n;
    d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = 0;

    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[BOMB TAG] bomb assign[ServerGamemode]d to p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode]=%z[ServerGamemode]\n",
        chos[ServerGamemode]n, [ServerGamemode][ServerGamemode]igib[ServerGamemode][ServerGamemode].siz[ServerGamemode]());
}

// B[ServerGamemode]oadcast bomb tag stat[ServerGamemode] to a[ServerGamemode][ServerGamemode] acti[ServerGamemode][ServerGamemode] c[ServerGamemode]i[ServerGamemode]nts.
[ServerGamemode]oid b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode]([ServerGamemode]OCKET sock,
                           [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                           const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                           [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    BombTag[ServerGamemode]tat[ServerGamemode]Pack[ServerGamemode]t pkt{};
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_BOMB_TAG_[ServerGamemode]TATE;
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].tick = 0;
    pkt.d[ServerGamemode][ServerGamemode][ServerGamemode]Id = d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;
    pkt.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    pkt.phas[ServerGamemode] = d.phas[ServerGamemode];
    pkt.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode];
    pkt.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    pkt.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x;
    pkt.tim[ServerGamemode][ServerGamemode]TicksR[ServerGamemode]maining = d.bombTim[ServerGamemode][ServerGamemode]Ticks;
    pkt.inacti[ServerGamemode][ServerGamemode]TicksR[ServerGamemode]maining = d.bombInacti[ServerGamemode][ServerGamemode]Ticks;
    pkt.s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = d.c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick;

    g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 bombPos = g[ServerGamemode]tBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode]Position(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, {});
    pkt.bombPosX = bombPos.x;
    pkt.bombPosY = bombPos.y;
    pkt.bombPosZ = bombPos.z;

    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            contin[ServerGamemode][ServerGamemode];
        const [ServerGamemode]int32_t [ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
        const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
            sock, const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond), &pkt, siz[ServerGamemode]of(pkt), [ServerGamemode][ServerGamemode][ServerGamemode]ntId,
            [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond)), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
            "[BOMB TAG] s[ServerGamemode]nt stat[ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]=%[ServerGamemode] phas[ServerGamemode]=%[ServerGamemode] own[ServerGamemode][ServerGamemode]=%[ServerGamemode] tim[ServerGamemode][ServerGamemode]=%[ServerGamemode] inacti[ServerGamemode][ServerGamemode]=%[ServerGamemode]\n",
            k[ServerGamemode].s[ServerGamemode]cond.id, d.phas[ServerGamemode], d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, d.bombTim[ServerGamemode][ServerGamemode]Ticks, d.bombInacti[ServerGamemode][ServerGamemode]Ticks);
    }
}

// B[ServerGamemode]oadcast pass [ServerGamemode]is[ServerGamemode]a[ServerGamemode]ization [ServerGamemode][ServerGamemode][ServerGamemode]nt to a[ServerGamemode][ServerGamemode] c[ServerGamemode]i[ServerGamemode]nts.
[ServerGamemode]oid b[ServerGamemode]oadcastBombTagPass([ServerGamemode]OCKET sock,
                          [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d,
                          const std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                          [ServerGamemode]int32_t o[ServerGamemode]dOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, [ServerGamemode]int32_t n[ServerGamemode]wOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id,
                          const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3& o[ServerGamemode]dPos, const g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3& n[ServerGamemode]wPos,
                          f[ServerGamemode]oat pass[ServerGamemode]ist, f[ServerGamemode]oat [ServerGamemode][ServerGamemode]wo[ServerGamemode]nd[ServerGamemode]ist,
                          [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    BombTagPassE[ServerGamemode][ServerGamemode]ntPack[ServerGamemode]t pkt{};
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].typ[ServerGamemode] = PACKET_BOMB_TAG_PA[ServerGamemode][ServerGamemode]_EVENT;
    pkt.h[ServerGamemode]ad[ServerGamemode][ServerGamemode].tick = 0;
    pkt.[ServerGamemode][ServerGamemode][ServerGamemode]ntId = n[ServerGamemode]xtR[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntId();
    pkt.[ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionId = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]E[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionId();
    pkt.s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = d.c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick;
    pkt.o[ServerGamemode]dOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = o[ServerGamemode]dOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    pkt.n[ServerGamemode]wOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = n[ServerGamemode]wOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
    pkt.o[ServerGamemode]dBombPosX = o[ServerGamemode]dPos.x;
    pkt.o[ServerGamemode]dBombPosY = o[ServerGamemode]dPos.y;
    pkt.o[ServerGamemode]dBombPosZ = o[ServerGamemode]dPos.z;
    pkt.n[ServerGamemode]wBombPosX = n[ServerGamemode]wPos.x;
    pkt.n[ServerGamemode]wBombPosY = n[ServerGamemode]wPos.y;
    pkt.n[ServerGamemode]wBombPosZ = n[ServerGamemode]wPos.z;
    pkt.pass[ServerGamemode]istanc[ServerGamemode] = pass[ServerGamemode]ist;
    pkt.s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]wo[ServerGamemode]nd[ServerGamemode]istanc[ServerGamemode] = [ServerGamemode][ServerGamemode]wo[ServerGamemode]nd[ServerGamemode]ist;
    pkt.acc[ServerGamemode]pt[ServerGamemode]d = 1;

    fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
        if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode])
            contin[ServerGamemode][ServerGamemode];
        const R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntQ[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]s[ServerGamemode][ServerGamemode]t [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]t = q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]ntToP[ServerGamemode]ay[ServerGamemode][ServerGamemode](
            sock, const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond), &pkt, siz[ServerGamemode]of(pkt), pkt.[ServerGamemode][ServerGamemode][ServerGamemode]ntId,
            [ServerGamemode][ServerGamemode][ServerGamemode]iab[ServerGamemode][ServerGamemode]Gam[ServerGamemode]p[ServerGamemode]ayE[ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode]ssionFo[ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode](const_cast<[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]&>(k[ServerGamemode].s[ServerGamemode]cond)), tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
    }
    ++d.bombPassCo[ServerGamemode]nt[ServerGamemode][ServerGamemode];
    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[BOMB TAG PA[ServerGamemode][ServerGamemode]] o[ServerGamemode]d=%[ServerGamemode] n[ServerGamemode]w=%[ServerGamemode] dist=%.2f [ServerGamemode][ServerGamemode]wo[ServerGamemode]nd=%.2f pass[ServerGamemode]s=%[ServerGamemode]\n",
        o[ServerGamemode]dOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, n[ServerGamemode]wOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, pass[ServerGamemode]ist, [ServerGamemode][ServerGamemode]wo[ServerGamemode]nd[ServerGamemode]ist, d.bombPassCo[ServerGamemode]nt[ServerGamemode][ServerGamemode]);
}

} // anonymo[ServerGamemode]s nam[ServerGamemode]spac[ServerGamemode]

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTag[ServerGamemode]ta[ServerGamemode]tMatch(boo[ServerGamemode] skipInt[ServerGamemode][ServerGamemode]mission)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;

    // Load bomb tag gam[ServerGamemode]mod[ServerGamemode] config [ServerGamemode]sing th[ServerGamemode] gam[ServerGamemode]mod[ServerGamemode]_id f[ServerGamemode]om th[ServerGamemode] J[ServerGamemode]ON.
    const Gam[ServerGamemode]mod[ServerGamemode]& gm = Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]gist[ServerGamemode]y::instanc[ServerGamemode]().g[ServerGamemode]t("bombtag");
    d.bombTim[ServerGamemode][ServerGamemode]TicksMax = ([ServerGamemode]int32_t)(gm.bombTim[ServerGamemode][ServerGamemode]Ticks > 0 ? gm.bombTim[ServerGamemode][ServerGamemode]Ticks : 900);
    d.bombInacti[ServerGamemode][ServerGamemode]TicksMax = ([ServerGamemode]int32_t)(gm.inacti[ServerGamemode][ServerGamemode]Ticks > 0 ? gm.inacti[ServerGamemode][ServerGamemode]Ticks : 60);
    d.bombB[ServerGamemode]inkTicks = ([ServerGamemode]int32_t)(gm.b[ServerGamemode]inkTicks > 0 ? gm.b[ServerGamemode]inkTicks : 30);
    d.bombMaxPass[ServerGamemode]anity[ServerGamemode]ist = gm.maxPass[ServerGamemode]anity[ServerGamemode]istanc[ServerGamemode] > 0.0f ? gm.maxPass[ServerGamemode]anity[ServerGamemode]istanc[ServerGamemode] : 3.0f;
    d.bombTagActi[ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.bombPassCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;
    d.bombExp[ServerGamemode]osionCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;
    d.bombTim[ServerGamemode][ServerGamemode]Ticks = d.bombTim[ServerGamemode][ServerGamemode]TicksMax;
    d.bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;
    d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 0;
    d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = 0;

    // [ServerGamemode]tanda[ServerGamemode]d match [ServerGamemode]if[ServerGamemode]cyc[ServerGamemode][ServerGamemode]
    d.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds = gm.co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds;
    d.go[ServerGamemode][ServerGamemode]conds = gm.go[ServerGamemode][ServerGamemode]conds;
    d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds = (f[ServerGamemode]oat)gm.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    d.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds = (f[ServerGamemode]oat)gm.[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds;
    d.tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds = 0;  // infinit[ServerGamemode]
    d.mapOn[ServerGamemode]y = fa[ServerGamemode]s[ServerGamemode];
    d.[ServerGamemode]astB[ServerGamemode]oadcastTick = 0;
    d.stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = t[ServerGamemode][ServerGamemode][ServerGamemode];
    d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION;
    d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = skipInt[ServerGamemode][ServerGamemode]mission ? 0.0f : d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
    d.matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    d.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s = gm.spawnOffs[ServerGamemode]tRadi[ServerGamemode]s;
    ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
    ++d.d[ServerGamemode][ServerGamemode][ServerGamemode]Id;

    [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
        "[BOMB TAG] match sta[ServerGamemode]ting tim[ServerGamemode][ServerGamemode]Ticks=%[ServerGamemode] inacti[ServerGamemode][ServerGamemode]Ticks=%[ServerGamemode] b[ServerGamemode]inkTicks=%[ServerGamemode] sanity[ServerGamemode]ist=%.1f\n",
        d.bombTim[ServerGamemode][ServerGamemode]TicksMax, d.bombInacti[ServerGamemode][ServerGamemode]TicksMax, d.bombB[ServerGamemode]inkTicks, d.bombMaxPass[ServerGamemode]anity[ServerGamemode]ist);
}

[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTagTick([ServerGamemode]OCKET sock,
                       std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                       H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                       std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                       Npc[ServerGamemode]yst[ServerGamemode]m& npc[ServerGamemode]yst[ServerGamemode]m,
                       [ServerGamemode]int32_t tick,
                       [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t)
{
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& d = s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();
    if (!d.[ServerGamemode]nab[ServerGamemode][ServerGamemode]d || !d.bombTagActi[ServerGamemode][ServerGamemode]) [ServerGamemode][ServerGamemode]t[ServerGamemode][ServerGamemode]n;
    d.c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = tick;

    // ── Hand[ServerGamemode][ServerGamemode] p[ServerGamemode]nding ki[ServerGamemode][ServerGamemode] f[ServerGamemode]om [ServerGamemode]xp[ServerGamemode]osion ───────────────────────────
    if (d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode])
    {
        d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
        const [ServerGamemode]int32_t [ServerGamemode]ictimId = d.p[ServerGamemode]ndingVictimId;
        // Instant [ServerGamemode][ServerGamemode]spawn at spawn point
        a[ServerGamemode]to [ServerGamemode]ictimIt = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find([ServerGamemode]ictimId);
        if ([ServerGamemode]ictimIt != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) {
            [ServerGamemode]ictimIt->s[ServerGamemode]cond.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
            [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
        }
    }

    // ── [ServerGamemode]tat[ServerGamemode] machin[ServerGamemode] ────────────────────────────────────────────────
    switch (d.phas[ServerGamemode]) {
    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING:
        if (co[ServerGamemode]ntActi[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]s(p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) >= 1) {
            // [ServerGamemode]ta[ServerGamemode]t bomb tag with a[ServerGamemode]ai[ServerGamemode]ab[ServerGamemode][ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s
            if (wo[ServerGamemode][ServerGamemode]d.spawnPoints.[ServerGamemode]mpty())
                assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
            assignMatchPa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, &npcs);
            b[ServerGamemode]ginMatchCo[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tick);
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[BOMB TAG] co[ServerGamemode]ntdown sta[ServerGamemode]t[ServerGamemode]d pa[ServerGamemode]ticipants=%z[ServerGamemode]\n", d.pa[ServerGamemode]ticipants.siz[ServerGamemode]());
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_COUNT[ServerGamemode]OWN:
        if (tick >= d.match[ServerGamemode]ta[ServerGamemode]tTick) {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_GO;
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.go[ServerGamemode][ServerGamemode]conds;
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[BOMB TAG] GO shown tick=%[ServerGamemode]\n", tick);
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_GO:
        d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
        if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f) {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE;
            d.match[ServerGamemode]ta[ServerGamemode]tTick = tick;
            d.bombTim[ServerGamemode][ServerGamemode]Ticks = d.bombTim[ServerGamemode][ServerGamemode]TicksMax;
            d.bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;
            // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ct fi[ServerGamemode]st bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode]
            s[ServerGamemode][ServerGamemode][ServerGamemode]ctN[ServerGamemode]wBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode](d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            [ServerGamemode][ServerGamemode]spawnA[ServerGamemode][ServerGamemode]Pa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                "[BOMB TAG] ACTIVE tick=%[ServerGamemode] ho[ServerGamemode]d[ServerGamemode][ServerGamemode]=%[ServerGamemode] tim[ServerGamemode][ServerGamemode]Ticks=%[ServerGamemode]\n",
                tick, d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, d.bombTim[ServerGamemode][ServerGamemode]Ticks);
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_ACTIVE:
    {
        // ── Bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode] disconn[ServerGamemode]ct[ServerGamemode]d o[ServerGamemode] di[ServerGamemode]d? T[ServerGamemode]ansf[ServerGamemode][ServerGamemode] imm[ServerGamemode]diat[ServerGamemode][ServerGamemode]y ───
        if (d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] == 1 && d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id != 0) {
            a[ServerGamemode]to ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
            if (ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It == p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() || ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]
                || ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.d[ServerGamemode]ad) {
                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[BOMB TAG] ho[ServerGamemode]d[ServerGamemode][ServerGamemode] [ServerGamemode]ost id=%[ServerGamemode] — t[ServerGamemode]ansf[ServerGamemode][ServerGamemode][ServerGamemode]ing\n",
                    d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
                d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 0;
                d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
                d.bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;
                s[ServerGamemode][ServerGamemode][ServerGamemode]ctN[ServerGamemode]wBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode](d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);
                ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            }
        }

        // ── Bomb tim[ServerGamemode][ServerGamemode] co[ServerGamemode]ntdown ──────────────────────────────────
        if (d.bombTim[ServerGamemode][ServerGamemode]Ticks > 0)
            --d.bombTim[ServerGamemode][ServerGamemode]Ticks;

        // ── Inacti[ServerGamemode][ServerGamemode] g[ServerGamemode]ac[ServerGamemode] p[ServerGamemode][ServerGamemode]iod ─────────────────────────────────
        if (d.bombInacti[ServerGamemode][ServerGamemode]Ticks > 0)
            --d.bombInacti[ServerGamemode][ServerGamemode]Ticks;

        // ── Bomb [ServerGamemode]xp[ServerGamemode]osion ────────────────────────────────────────
        if (d.bombTim[ServerGamemode][ServerGamemode]Ticks == 0 && d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] != 0) {
            // Ki[ServerGamemode][ServerGamemode] th[ServerGamemode] bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode]
            [ServerGamemode]int32_t [ServerGamemode]ictimId = d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id;
            a[ServerGamemode]to [ServerGamemode]ictimIt = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find([ServerGamemode]ictimId);
            if ([ServerGamemode]ictimIt != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() && ![ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode]ad) {
                // App[ServerGamemode]y [ServerGamemode][ServerGamemode]tha[ServerGamemode] damag[ServerGamemode] [ServerGamemode]ia th[ServerGamemode] no[ServerGamemode]ma[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] damag[ServerGamemode] path
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.h[ServerGamemode]a[ServerGamemode]th = 0;
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode]ad = t[ServerGamemode][ServerGamemode][ServerGamemode];
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.01f;
                ++[ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode]aths;

                // C[ServerGamemode][ServerGamemode]dit ki[ServerGamemode][ServerGamemode] to a [ServerGamemode]andom oth[ServerGamemode][ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode] ([ServerGamemode]xp[ServerGamemode]osion is [ServerGamemode]n[ServerGamemode]i[ServerGamemode]onm[ServerGamemode]nt)
                // Fo[ServerGamemode] bomb tag, w[ServerGamemode] c[ServerGamemode][ServerGamemode]dit no on[ServerGamemode] — bomb [ServerGamemode]xp[ServerGamemode]osion is [ServerGamemode]n[ServerGamemode]i[ServerGamemode]onm[ServerGamemode]nta[ServerGamemode]
                d.hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = t[ServerGamemode][ServerGamemode][ServerGamemode];
                d.p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = 0;
                d.p[ServerGamemode]ndingVictimId = [ServerGamemode]ictimId;

                [ServerGamemode][ServerGamemode]b[ServerGamemode]g::wa[ServerGamemode]n([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                    "[BOMB TAG] [ServerGamemode]xp[ServerGamemode]osion! [ServerGamemode]ictim=%[ServerGamemode] d[ServerGamemode]aths=%[ServerGamemode] [ServerGamemode]xp[ServerGamemode]osions=%[ServerGamemode]\n",
                    [ServerGamemode]ictimId, [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode]aths, d.bombExp[ServerGamemode]osionCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] + 1);
            }
            ++d.bombExp[ServerGamemode]osionCo[ServerGamemode]nt[ServerGamemode][ServerGamemode];

            // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ct n[ServerGamemode]w bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode] and [ServerGamemode][ServerGamemode]s[ServerGamemode]t tim[ServerGamemode][ServerGamemode]
            d.bombTim[ServerGamemode][ServerGamemode]Ticks = d.bombTim[ServerGamemode][ServerGamemode]TicksMax;
            d.bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;
            s[ServerGamemode][ServerGamemode][ServerGamemode]ctN[ServerGamemode]wBombHo[ServerGamemode]d[ServerGamemode][ServerGamemode](d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s);

            // R[ServerGamemode]spawn th[ServerGamemode] ki[ServerGamemode][ServerGamemode][ServerGamemode]d p[ServerGamemode]ay[ServerGamemode][ServerGamemode] instant[ServerGamemode]y
            if ([ServerGamemode]ictimIt != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd()) {
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode]ad = fa[ServerGamemode]s[ServerGamemode];
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.h[ServerGamemode]a[ServerGamemode]th = 100;
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos = gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawnPoint(d);
                [ServerGamemode]ictimIt->s[ServerGamemode]cond.[ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]conds = 0.0f;
                b[ServerGamemode]ginA[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode]T[ServerGamemode]ansfo[ServerGamemode]m([ServerGamemode]ictimIt->s[ServerGamemode]cond,
                    [ServerGamemode]ictimIt->s[ServerGamemode]cond.d[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]pawnPos, g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3(0.0f),
                    [ServerGamemode]ictimIt->s[ServerGamemode]cond.yaw, "bomb-[ServerGamemode][ServerGamemode]spawn");
            }

            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode][ServerGamemode]ak;
        }

        // ── Physica[ServerGamemode] contact d[ServerGamemode]t[ServerGamemode]ction (bomb pass) ────────────────
        if (d.bombInacti[ServerGamemode][ServerGamemode]Ticks == 0 && d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] == 1 && !d.matchO[ServerGamemode][ServerGamemode][ServerGamemode]) {
            // P[ServerGamemode]ay[ServerGamemode][ServerGamemode] ho[ServerGamemode]ds bomb — ch[ServerGamemode]ck contact with oth[ServerGamemode][ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s
            a[ServerGamemode]to ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It = p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.find(d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
            if (ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It != p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s.[ServerGamemode]nd() && !ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond.d[ServerGamemode]ad) {
                g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 ho[ServerGamemode]d[ServerGamemode][ServerGamemode]Pos = g[ServerGamemode]tEntityRootPos(ho[ServerGamemode]d[ServerGamemode][ServerGamemode]It->s[ServerGamemode]cond);
                f[ServerGamemode]oat bombRadi[ServerGamemode]s = 0.5f;
                f[ServerGamemode]oat ta[ServerGamemode]g[ServerGamemode]tRadi[ServerGamemode]s = 1.5f;

                fo[ServerGamemode] (const a[ServerGamemode]to& k[ServerGamemode] : p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s) {
                    if (k[ServerGamemode].fi[ServerGamemode]st == d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id) contin[ServerGamemode][ServerGamemode];
                    if (k[ServerGamemode].s[ServerGamemode]cond.spawn[ServerGamemode]tat[ServerGamemode] != [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]::Acti[ServerGamemode][ServerGamemode]) contin[ServerGamemode][ServerGamemode];
                    if (k[ServerGamemode].s[ServerGamemode]cond.d[ServerGamemode]ad) contin[ServerGamemode][ServerGamemode];

                    g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 ta[ServerGamemode]g[ServerGamemode]tPos = g[ServerGamemode]tEntityRootPos(k[ServerGamemode].s[ServerGamemode]cond);
                    f[ServerGamemode]oat dist = g[ServerGamemode]m::[ServerGamemode][ServerGamemode]ngth(ho[ServerGamemode]d[ServerGamemode][ServerGamemode]Pos - ta[ServerGamemode]g[ServerGamemode]tPos);

                    // [ServerGamemode]anity distanc[ServerGamemode] ch[ServerGamemode]ck
                    if (dist > d.bombMaxPass[ServerGamemode]anity[ServerGamemode]ist) contin[ServerGamemode][ServerGamemode];

                    // Physica[ServerGamemode] o[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ap ch[ServerGamemode]ck
                    if (bomb[ServerGamemode]ph[ServerGamemode][ServerGamemode][ServerGamemode]O[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ap(ho[ServerGamemode]d[ServerGamemode][ServerGamemode]Pos, bombRadi[ServerGamemode]s, ta[ServerGamemode]g[ServerGamemode]tPos, ta[ServerGamemode]g[ServerGamemode]tRadi[ServerGamemode]s)) {
                        // T[ServerGamemode]ansf[ServerGamemode][ServerGamemode] bomb
                        g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 o[ServerGamemode]dPos = ho[ServerGamemode]d[ServerGamemode][ServerGamemode]Pos;
                        d.bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 1;
                        d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = k[ServerGamemode].fi[ServerGamemode]st;
                        d.bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = 0;
                        d.bombInacti[ServerGamemode][ServerGamemode]Ticks = d.bombInacti[ServerGamemode][ServerGamemode]TicksMax;

                        g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 n[ServerGamemode]wPos = g[ServerGamemode]tEntityRootPos(k[ServerGamemode].s[ServerGamemode]cond);
                        b[ServerGamemode]oadcastBombTagPass(sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                            d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, k[ServerGamemode].fi[ServerGamemode]st,
                            o[ServerGamemode]dPos, n[ServerGamemode]wPos, dist, dist, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                        ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
                        b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
                        [ServerGamemode][ServerGamemode]b[ServerGamemode]g::[ServerGamemode]og([ServerGamemode][ServerGamemode]b[ServerGamemode]g::Cat[ServerGamemode]go[ServerGamemode]y::[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode],
                            "[BOMB TAG] PA[ServerGamemode][ServerGamemode] %[ServerGamemode] -> %[ServerGamemode] dist=%.2f\n",
                            d.bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, k[ServerGamemode].fi[ServerGamemode]st, dist);
                        b[ServerGamemode][ServerGamemode]ak;
                    }
                }
            }
        }

        // ── P[ServerGamemode][ServerGamemode]iodic stat[ServerGamemode] b[ServerGamemode]oadcast ──────────────────────────────
        if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 10) {
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;
    }

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_RE[ServerGamemode]ULT[ServerGamemode]:
        d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
        if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f) {
            d.phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION;
            d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = d.int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds;
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60) {
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    cas[ServerGamemode] [ServerGamemode]UEL_PHA[ServerGamemode]E_INTERMI[ServerGamemode][ServerGamemode]ION:
        d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] -= [ServerGamemode]ERVER_[ServerGamemode]T;
        if (d.phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] <= 0.0f) {
            assignGam[ServerGamemode]mod[ServerGamemode][ServerGamemode]pawns(d, wo[ServerGamemode][ServerGamemode]d);
            assignMatchPa[ServerGamemode]ticipants(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, &npcs);
            b[ServerGamemode]ginMatchCo[ServerGamemode]ntdown(d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tick);
            d.bombTim[ServerGamemode][ServerGamemode]Ticks = d.bombTim[ServerGamemode][ServerGamemode]TicksMax;
            d.bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;
            ++d.stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
            b[ServerGamemode]oadcastBombTag[ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        } [ServerGamemode][ServerGamemode]s[ServerGamemode] if (tick - d.[ServerGamemode]astB[ServerGamemode]oadcastTick >= 60) {
            d.[ServerGamemode]astB[ServerGamemode]oadcastTick = tick;
            b[ServerGamemode]oadcast[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode](sock, d, p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);
        }
        b[ServerGamemode][ServerGamemode]ak;

    d[ServerGamemode]fa[ServerGamemode][ServerGamemode]t:
        b[ServerGamemode][ServerGamemode]ak;
    }
}

} // nam[ServerGamemode]spac[ServerGamemode] MimitaN[ServerGamemode]t
