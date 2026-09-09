// 09 01 2026, 00 00
/* p[ServerGamemode][ServerGamemode]pos[ServerGamemode]
* [ServerGamemode][ServerGamemode]c[ServerGamemode]a[ServerGamemode][ServerGamemode]s th[ServerGamemode] a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] J[ServerGamemode]ON-d[ServerGamemode]fin[ServerGamemode]d gam[ServerGamemode]mod[ServerGamemode] stat[ServerGamemode] and tick [ServerGamemode]nt[ServerGamemode]y points.
* R[ServerGamemode]ns sha[ServerGamemode][ServerGamemode]d waiting, int[ServerGamemode][ServerGamemode]mission, co[ServerGamemode]ntdown, acti[ServerGamemode][ServerGamemode], [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts, and [ServerGamemode][ServerGamemode]match [ServerGamemode]if[ServerGamemode]cyc[ServerGamemode][ServerGamemode].
* [ServerGamemode][ServerGamemode]ppo[ServerGamemode]ts d[ServerGamemode][ServerGamemode][ServerGamemode], FFA, T[ServerGamemode]M, Bomb Tag, sandbox, and f[ServerGamemode]t[ServerGamemode][ServerGamemode][ServerGamemode] gam[ServerGamemode]mod[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] s[ServerGamemode]ts.
* [ServerGamemode]o[ServerGamemode]s NOT sim[ServerGamemode][ServerGamemode]at[ServerGamemode] p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s, app[ServerGamemode]y damag[ServerGamemode], o[ServerGamemode] [ServerGamemode][ServerGamemode]nd[ServerGamemode][ServerGamemode] anything.
* [ServerGamemode]o[ServerGamemode]s NOT own th[ServerGamemode] c[ServerGamemode]i[ServerGamemode]nt q[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]/matchmaking o[ServerGamemode] th[ServerGamemode] coo[ServerGamemode]dinato[ServerGamemode] p[ServerGamemode]otoco[ServerGamemode].
* [ServerGamemode]o[ServerGamemode]s NOT c[ServerGamemode][ServerGamemode]at[ServerGamemode] t[ServerGamemode]am spawns - it [ServerGamemode][ServerGamemode]ads th[ServerGamemode]m f[ServerGamemode]om th[ServerGamemode] [ServerGamemode]oad[ServerGamemode]d h[ServerGamemode]ad[ServerGamemode][ServerGamemode]ss wo[ServerGamemode][ServerGamemode]d.
*/

#p[ServerGamemode]agma onc[ServerGamemode]

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <cstdint>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <st[ServerGamemode]ing>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_s[ServerGamemode]t>
#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <[ServerGamemode][ServerGamemode]cto[ServerGamemode]>

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] <g[ServerGamemode]m/g[ServerGamemode]m.hpp>

#inc[ServerGamemode][ServerGamemode]d[ServerGamemode] "n[ServerGamemode]two[ServerGamemode]k/s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].h"

nam[ServerGamemode]spac[ServerGamemode] MimitaN[ServerGamemode]t {

[ServerGamemode]n[ServerGamemode]m c[ServerGamemode]ass [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode] { [ServerGamemode]andbox, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode], T[ServerGamemode]am[ServerGamemode][ServerGamemode]athmatch, F[ServerGamemode][ServerGamemode][ServerGamemode]Fo[ServerGamemode]A[ServerGamemode][ServerGamemode] };

st[ServerGamemode][ServerGamemode]ct [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]
{
    boo[ServerGamemode] [ServerGamemode]nab[ServerGamemode][ServerGamemode]d = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode] mod[ServerGamemode] = [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Mod[ServerGamemode]::[ServerGamemode]andbox;
    boo[ServerGamemode] mapOn[ServerGamemode]y = fa[ServerGamemode]s[ServerGamemode];
    std::st[ServerGamemode]ing comm[ServerGamemode]nityMod[ServerGamemode] = "sandbox";
    int comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId = 1;
    boo[ServerGamemode] comm[ServerGamemode]nityW[ServerGamemode]apon[ServerGamemode][ServerGamemode]tExp[ServerGamemode]icit = fa[ServerGamemode]s[ServerGamemode];
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, int> comm[ServerGamemode]nity[ServerGamemode]co[ServerGamemode][ServerGamemode]s;
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, int> comm[ServerGamemode]nityT[ServerGamemode]ams;
    int comm[ServerGamemode]nityT[ServerGamemode]am[ServerGamemode]co[ServerGamemode][ServerGamemode][2] = {0, 0};
    boo[ServerGamemode] comm[ServerGamemode]nityRo[ServerGamemode]ndO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode]int64_t comm[ServerGamemode]nityRo[ServerGamemode]ndR[ServerGamemode]s[ServerGamemode]tMs = 0;
    // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode]Phas[ServerGamemode] (pack[ServerGamemode]ts.h)
    [ServerGamemode]int8_t phas[ServerGamemode] = [ServerGamemode]UEL_PHA[ServerGamemode]E_WAITING;
    boo[ServerGamemode] matchO[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    int sco[ServerGamemode][ServerGamemode]A = 0;
    int sco[ServerGamemode][ServerGamemode]B = 0;
    int goa[ServerGamemode]Va[ServerGamemode][ServerGamemode][ServerGamemode] = 20;
    f[ServerGamemode]oat co[ServerGamemode]ntdown = 0.0f;
    f[ServerGamemode]oat co[ServerGamemode]ntdown[ServerGamemode][ServerGamemode]conds = 3.0f;
    f[ServerGamemode]oat go[ServerGamemode][ServerGamemode]conds = 0.75f;
    f[ServerGamemode]oat [ServerGamemode][ServerGamemode]matchL[ServerGamemode]ft = 0.0f;
    f[ServerGamemode]oat [ServerGamemode][ServerGamemode]match[ServerGamemode][ServerGamemode]conds = 5.0f;
    std::st[ServerGamemode]ing t[ServerGamemode]amANam[ServerGamemode] = "RE[ServerGamemode]";
    std::st[ServerGamemode]ing t[ServerGamemode]amBNam[ServerGamemode] = "BLUE";
    [ServerGamemode]int32_t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]AId = 0;
    [ServerGamemode]int32_t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]BId = 0;
    [ServerGamemode]int32_t winn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;
    // Th[ServerGamemode] sing[ServerGamemode][ServerGamemode] match ancho[ServerGamemode]: both t[ServerGamemode]ams a[ServerGamemode]ways spawn n[ServerGamemode]a[ServerGamemode] this on[ServerGamemode] point
    // (pick[ServerGamemode]d f[ServerGamemode]om th[ServerGamemode] map's spawn points, fix[ServerGamemode]d fo[ServerGamemode] th[ServerGamemode] who[ServerGamemode][ServerGamemode] match), with a
    // f[ServerGamemode][ServerGamemode]sh [ServerGamemode]andom XY offs[ServerGamemode]t on [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y spawn/[ServerGamemode][ServerGamemode]spawn. F[ServerGamemode]oating on p[ServerGamemode][ServerGamemode]pos[ServerGamemode].
    g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 spawnA{0.0f};
    g[ServerGamemode]m::[ServerGamemode][ServerGamemode]c3 spawnB{0.0f};
    // Random XY offs[ServerGamemode]t [ServerGamemode]adi[ServerGamemode]s a[ServerGamemode]o[ServerGamemode]nd th[ServerGamemode] ancho[ServerGamemode] (m[ServerGamemode]t[ServerGamemode][ServerGamemode]s).
    f[ServerGamemode]oat spawnOffs[ServerGamemode]tRadi[ServerGamemode]s = 5.0f;
    boo[ServerGamemode] spawnsAssign[ServerGamemode]d = fa[ServerGamemode]s[ServerGamemode];
    // Th[ServerGamemode] [ServerGamemode]ast [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode]Pack[ServerGamemode]t s[ServerGamemode]nt, to a[ServerGamemode]oid [ServerGamemode][ServerGamemode]-b[ServerGamemode]oadcasting id[ServerGamemode]ntica[ServerGamemode] stat[ServerGamemode].
    boo[ServerGamemode] stat[ServerGamemode][ServerGamemode][ServerGamemode]nt = fa[ServerGamemode]s[ServerGamemode];
    // P[ServerGamemode]nding ki[ServerGamemode][ServerGamemode] d[ServerGamemode]f[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]d f[ServerGamemode]om app[ServerGamemode]y[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]amag[ServerGamemode] (no sock[ServerGamemode]t th[ServerGamemode][ServerGamemode][ServerGamemode]). P[ServerGamemode]oc[ServerGamemode]ss[ServerGamemode]d
    // at th[ServerGamemode] n[ServerGamemode]xt s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]Tick, which has th[ServerGamemode] sock[ServerGamemode]t + pack[ServerGamemode]t co[ServerGamemode]nt[ServerGamemode][ServerGamemode]s.
    boo[ServerGamemode] hasP[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode]int32_t p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Id = 0;
    [ServerGamemode]int32_t p[ServerGamemode]ndingVictimId = 0;
    boo[ServerGamemode] p[ServerGamemode]ndingKi[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]IsNpc = fa[ServerGamemode]s[ServerGamemode];
    // P[ServerGamemode][ServerGamemode]iodic [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]tat[ServerGamemode] b[ServerGamemode]oadcast cad[ServerGamemode]nc[ServerGamemode] so c[ServerGamemode]i[ServerGamemode]nts can d[ServerGamemode]t[ServerGamemode]ct a d[ServerGamemode]ad s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode].
    [ServerGamemode]int32_t [ServerGamemode]astB[ServerGamemode]oadcastTick = 0;
    // Fo[ServerGamemode]c[ServerGamemode]s th[ServerGamemode] fi[ServerGamemode]st a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] comm[ServerGamemode]nity-match stat[ServerGamemode] to [ServerGamemode][ServerGamemode]ach c[ServerGamemode]i[ServerGamemode]nts
    // imm[ServerGamemode]diat[ServerGamemode][ServerGamemode]y aft[ServerGamemode][ServerGamemode] mod[ServerGamemode]sta[ServerGamemode]t/mod[ServerGamemode]sta[ServerGamemode]tnow chang[ServerGamemode]s th[ServerGamemode] [ServerGamemode][ServerGamemode]ntim[ServerGamemode] mod[ServerGamemode].
    boo[ServerGamemode] stat[ServerGamemode]B[ServerGamemode]oadcastP[ServerGamemode]nding = fa[ServerGamemode]s[ServerGamemode];
    // Li[ServerGamemode][ServerGamemode] map [ServerGamemode]otation (a[ServerGamemode]to on [ServerGamemode][ServerGamemode]match) + man[ServerGamemode]a[ServerGamemode] chang[ServerGamemode]map [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st.
    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing> mapPoo[ServerGamemode];
    boo[ServerGamemode] [ServerGamemode]otat[ServerGamemode]Maps = fa[ServerGamemode]s[ServerGamemode];
    boo[ServerGamemode] a[ServerGamemode]toMapRotation = fa[ServerGamemode]s[ServerGamemode];
    [ServerGamemode]int32_t mapRotationMin[ServerGamemode]t[ServerGamemode]s = 15;
    [ServerGamemode]int64_t n[ServerGamemode]xtMapRotationMs = 0;
    [ServerGamemode]int64_t mapChang[ServerGamemode]Co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tMs = 0;
    std::st[ServerGamemode]ing p[ServerGamemode]ndingA[ServerGamemode]tomaticMap;
    boo[ServerGamemode] hasP[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map = fa[ServerGamemode]s[ServerGamemode];
    std::st[ServerGamemode]ing p[ServerGamemode]ndingMan[ServerGamemode]a[ServerGamemode]Map;
    // Maps a[ServerGamemode][ServerGamemode][ServerGamemode]ady [ServerGamemode]s[ServerGamemode]d this [ServerGamemode]otation cyc[ServerGamemode][ServerGamemode] (so [ServerGamemode]ach n[ServerGamemode]w d[ServerGamemode][ServerGamemode][ServerGamemode] picks a map w[ServerGamemode]
    // w[ServerGamemode][ServerGamemode][ServerGamemode]n't j[ServerGamemode]st on, and n[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]p[ServerGamemode]ats [ServerGamemode]nti[ServerGamemode] th[ServerGamemode] who[ServerGamemode][ServerGamemode] poo[ServerGamemode] is [ServerGamemode]s[ServerGamemode]d).
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_s[ServerGamemode]t<std::st[ServerGamemode]ing> [ServerGamemode]s[ServerGamemode]dMaps;
    // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]'s c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt [ServerGamemode]oad[ServerGamemode]d map (nam[ServerGamemode] on[ServerGamemode]y, fo[ServerGamemode] HU[ServerGamemode]/[ServerGamemode]ogging).
    std::st[ServerGamemode]ing mapId;
    [ServerGamemode]int32_t d[ServerGamemode][ServerGamemode][ServerGamemode]Id = 0;
    [ServerGamemode]int32_t mapV[ServerGamemode][ServerGamemode]sion = 0;
    [ServerGamemode]int32_t spawnAncho[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = 0;
    [ServerGamemode]int32_t [ServerGamemode][ServerGamemode]spawn[ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]nc[ServerGamemode] = 0;
    [ServerGamemode]int32_t stat[ServerGamemode]V[ServerGamemode][ServerGamemode]sion = 0;
    [ServerGamemode]int32_t spawnAncho[ServerGamemode]Ind[ServerGamemode]x = 0;

    // ── FFA/T[ServerGamemode]M match mod[ServerGamemode] fi[ServerGamemode][ServerGamemode]ds ──────────────────────────────────
    // Match mod[ServerGamemode]: "d[ServerGamemode][ServerGamemode][ServerGamemode]", "ffa", "tdm"
    std::st[ServerGamemode]ing matchMod[ServerGamemode] = "d[ServerGamemode][ServerGamemode][ServerGamemode]";

    // A[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] tick [ServerGamemode][ServerGamemode]f[ServerGamemode][ServerGamemode][ServerGamemode]nc[ServerGamemode]s fo[ServerGamemode] co[ServerGamemode]ntdown/sta[ServerGamemode]t/[ServerGamemode]nd
    [ServerGamemode]int32_t co[ServerGamemode]ntdown[ServerGamemode]ta[ServerGamemode]tTick = 0;
    [ServerGamemode]int32_t match[ServerGamemode]ta[ServerGamemode]tTick = 0;
    [ServerGamemode]int32_t matchTim[ServerGamemode]LimitTick = 0;  // match[ServerGamemode]ta[ServerGamemode]tTick + tim[ServerGamemode]LimitTicks
    [ServerGamemode]int32_t c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Tick = 0;

    // Int[ServerGamemode][ServerGamemode]mission/[ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts phas[ServerGamemode] tim[ServerGamemode][ServerGamemode]s
    f[ServerGamemode]oat phas[ServerGamemode]Tim[ServerGamemode][ServerGamemode] = 0.0f;
    boo[ServerGamemode] sta[ServerGamemode]tCo[ServerGamemode]ntdownImm[ServerGamemode]diat[ServerGamemode][ServerGamemode]y = fa[ServerGamemode]s[ServerGamemode];
    f[ServerGamemode]oat int[ServerGamemode][ServerGamemode]mission[ServerGamemode][ServerGamemode]conds = 15.0f;
    f[ServerGamemode]oat [ServerGamemode][ServerGamemode]s[ServerGamemode][ServerGamemode]ts[ServerGamemode][ServerGamemode]conds = 8.0f;
    int tim[ServerGamemode]Limit[ServerGamemode][ServerGamemode]conds = 300;

    // FFA sco[ServerGamemode]ing: p[ServerGamemode][ServerGamemode]-p[ServerGamemode]ay[ServerGamemode][ServerGamemode] ki[ServerGamemode][ServerGamemode]s/d[ServerGamemode]aths
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, int> ffaKi[ServerGamemode][ServerGamemode]s;
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, int> ffa[ServerGamemode][ServerGamemode]aths;

    // T[ServerGamemode]M sco[ServerGamemode]ing
    int [ServerGamemode][ServerGamemode]dT[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;
    int b[ServerGamemode][ServerGamemode][ServerGamemode]T[ServerGamemode]amKi[ServerGamemode][ServerGamemode]s = 0;

    // T[ServerGamemode]am assignm[ServerGamemode]nts (p[ServerGamemode][ServerGamemode]sist[ServerGamemode]nt p[ServerGamemode][ServerGamemode] match, 0=[ServerGamemode][ServerGamemode]d, 1=b[ServerGamemode][ServerGamemode][ServerGamemode])
    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, int> matchT[ServerGamemode]ams;

    // A[ServerGamemode][ServerGamemode] pa[ServerGamemode]ticipating p[ServerGamemode]ay[ServerGamemode][ServerGamemode] I[ServerGamemode]s (FFA/T[ServerGamemode]M can ha[ServerGamemode][ServerGamemode] >2 p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s)
    std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<[ServerGamemode]int32_t> pa[ServerGamemode]ticipants;

    // Victo[ServerGamemode]y info
    int [ServerGamemode]icto[ServerGamemode]yTyp[ServerGamemode] = 0;  // 0=[ServerGamemode]co[ServerGamemode][ServerGamemode]Limit, 1=Tim[ServerGamemode]Limit
    int winn[ServerGamemode][ServerGamemode]T[ServerGamemode]am = -1;  // fo[ServerGamemode] T[ServerGamemode]M: 0=[ServerGamemode][ServerGamemode]d, 1=b[ServerGamemode][ServerGamemode][ServerGamemode]

    // Match [ServerGamemode][ServerGamemode][ServerGamemode]nt co[ServerGamemode]nt[ServerGamemode][ServerGamemode] fo[ServerGamemode] Ki[ServerGamemode][ServerGamemode]E[ServerGamemode][ServerGamemode]nt I[ServerGamemode]s
    [ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode]E[ServerGamemode][ServerGamemode]ntCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;

    // ── Bomb Tag fi[ServerGamemode][ServerGamemode]ds ─────────────────────────────────────────────
    // [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]-a[ServerGamemode]tho[ServerGamemode]itati[ServerGamemode][ServerGamemode] bomb own[ServerGamemode][ServerGamemode]ship, tim[ServerGamemode][ServerGamemode], and inacti[ServerGamemode][ServerGamemode] stat[ServerGamemode].
    // On[ServerGamemode]y acti[ServerGamemode][ServerGamemode] wh[ServerGamemode]n matchMod[ServerGamemode] == "bombtag".
    [ServerGamemode]int8_t bombOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] = 0;          // BombTagOwn[ServerGamemode][ServerGamemode]Typ[ServerGamemode] (0=non[ServerGamemode], 1=p[ServerGamemode]ay[ServerGamemode][ServerGamemode], 2=npc)
    [ServerGamemode]int32_t bombOwn[ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id = 0;     // P[ServerGamemode]ay[ServerGamemode][ServerGamemode] I[ServerGamemode] if own[ServerGamemode][ServerGamemode] is p[ServerGamemode]ay[ServerGamemode][ServerGamemode]
    [ServerGamemode]int32_t bombOwn[ServerGamemode][ServerGamemode]NpcInd[ServerGamemode]x = 0;     // NPC ind[ServerGamemode]x if own[ServerGamemode][ServerGamemode] is NPC
    [ServerGamemode]int32_t bombTim[ServerGamemode][ServerGamemode]Ticks = 0;        // R[ServerGamemode]maining ticks [ServerGamemode]nti[ServerGamemode] [ServerGamemode]xp[ServerGamemode]osion
    [ServerGamemode]int32_t bombInacti[ServerGamemode][ServerGamemode]Ticks = 0;     // Ticks [ServerGamemode][ServerGamemode]maining in inacti[ServerGamemode][ServerGamemode] g[ServerGamemode]ac[ServerGamemode]
    [ServerGamemode]int32_t bombTim[ServerGamemode][ServerGamemode]TicksMax = 900;   // Config: ticks p[ServerGamemode][ServerGamemode] bomb cyc[ServerGamemode][ServerGamemode] (15s * 60)
    [ServerGamemode]int32_t bombInacti[ServerGamemode][ServerGamemode]TicksMax = 60; // Config: inacti[ServerGamemode][ServerGamemode] g[ServerGamemode]ac[ServerGamemode] ticks aft[ServerGamemode][ServerGamemode] pass
    [ServerGamemode]int32_t bombB[ServerGamemode]inkTicks = 30;       // Config: ticks p[ServerGamemode][ServerGamemode] co[ServerGamemode]o[ServerGamemode] b[ServerGamemode]ink phas[ServerGamemode]
    f[ServerGamemode]oat bombMaxPass[ServerGamemode]anity[ServerGamemode]ist = 3.0f; // Config: ha[ServerGamemode]d [ServerGamemode][ServerGamemode]j[ServerGamemode]ction distanc[ServerGamemode] (m[ServerGamemode]t[ServerGamemode][ServerGamemode]s)
    boo[ServerGamemode] bombTagActi[ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];         // T[ServerGamemode][ServerGamemode][ServerGamemode] wh[ServerGamemode]n bomb tag match is [ServerGamemode][ServerGamemode]nning
    boo[ServerGamemode] hasBombF[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode] = fa[ServerGamemode]s[ServerGamemode];        // T[ServerGamemode][ServerGamemode][ServerGamemode] wh[ServerGamemode]n gam[ServerGamemode]mod[ServerGamemode] d[ServerGamemode]c[ServerGamemode]a[ServerGamemode][ServerGamemode]s bomb_ho[ServerGamemode]d[ServerGamemode][ServerGamemode]_t[ServerGamemode]xt f[ServerGamemode]at[ServerGamemode][ServerGamemode][ServerGamemode]
    boo[ServerGamemode] p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witch = fa[ServerGamemode]s[ServerGamemode];
    boo[ServerGamemode] p[ServerGamemode]ndingMod[ServerGamemode][ServerGamemode]witchCo[ServerGamemode]ntdown = fa[ServerGamemode]s[ServerGamemode];
    std::st[ServerGamemode]ing p[ServerGamemode]ndingGam[ServerGamemode]mod[ServerGamemode]Id;
    [ServerGamemode]int32_t bombPassCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;       // Tota[ServerGamemode] pass[ServerGamemode]s this s[ServerGamemode]ssion (fo[ServerGamemode] [ServerGamemode]ogging)
    [ServerGamemode]int32_t bombExp[ServerGamemode]osionCo[ServerGamemode]nt[ServerGamemode][ServerGamemode] = 0;  // Tota[ServerGamemode] [ServerGamemode]xp[ServerGamemode]osions this s[ServerGamemode]ssion
};

// [ServerGamemode]ing[ServerGamemode][ServerGamemode]ton gam[ServerGamemode]mod[ServerGamemode] stat[ServerGamemode] fo[ServerGamemode] th[ServerGamemode] c[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]nt s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] p[ServerGamemode]oc[ServerGamemode]ss.
[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]();

// [ServerGamemode]ta[ServerGamemode]t th[ServerGamemode] sha[ServerGamemode][ServerGamemode]d s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]ntim[ServerGamemode] with th[ServerGamemode] gi[ServerGamemode][ServerGamemode]n mod[ServerGamemode] [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s. [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode], FFA, T[ServerGamemode]M,
// and sandbox a[ServerGamemode][ServerGamemode] [ServerGamemode]s[ServerGamemode] this sam[ServerGamemode] [ServerGamemode]if[ServerGamemode]cyc[ServerGamemode][ServerGamemode] own[ServerGamemode][ServerGamemode].
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ta[ServerGamemode]tMod[ServerGamemode](const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s);

// Compatibi[ServerGamemode]ity [ServerGamemode]nt[ServerGamemode]y point fo[ServerGamemode] [ServerGamemode]xisting [ServerGamemode][ServerGamemode]gacy ca[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]ta[ServerGamemode]t(const [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode][ServerGamemode]tat[ServerGamemode]& [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]s);

// Ca[ServerGamemode][ServerGamemode][ServerGamemode]d [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] tick whi[ServerGamemode][ServerGamemode] th[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] [ServerGamemode][ServerGamemode]ns a manag[ServerGamemode]d gam[ServerGamemode]mod[ServerGamemode].
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]Tick([ServerGamemode]OCKET sock,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                    H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                    Wo[ServerGamemode][ServerGamemode]d& npcWo[ServerGamemode][ServerGamemode]d,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                    Npc[ServerGamemode]yst[ServerGamemode]m& npc[ServerGamemode]yst[ServerGamemode]m,
                    std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_s[ServerGamemode]t<[ServerGamemode]int32_t>& npcIdsA[ServerGamemode]i[ServerGamemode][ServerGamemode],
                    [ServerGamemode]int32_t tick,
                    [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);

// Ca[ServerGamemode][ServerGamemode][ServerGamemode]d f[ServerGamemode]om app[ServerGamemode]y[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]amag[ServerGamemode] wh[ServerGamemode]n a ki[ServerGamemode][ServerGamemode] is confi[ServerGamemode]m[ServerGamemode]d. Has no sock[ServerGamemode]t, so it
// on[ServerGamemode]y [ServerGamemode][ServerGamemode]co[ServerGamemode]ds th[ServerGamemode] ki[ServerGamemode][ServerGamemode] (instant [ServerGamemode][ServerGamemode]spawn + p[ServerGamemode]nding f[ServerGamemode]ag) and d[ServerGamemode]f[ServerGamemode][ServerGamemode]s sco[ServerGamemode][ServerGamemode] and
// t[ServerGamemode]ac[ServerGamemode][ServerGamemode] b[ServerGamemode]oadcast to th[ServerGamemode] n[ServerGamemode]xt s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]Tick.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]OnP[ServerGamemode]ay[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ath([ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id,
                             [ServerGamemode]int32_t [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]OnNpc[ServerGamemode][ServerGamemode]ath([ServerGamemode]int32_t ki[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]NpcId,
                          [ServerGamemode]int32_t [ServerGamemode]ictimP[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id);

// A p[ServerGamemode]ay[ServerGamemode][ServerGamemode] p[ServerGamemode][ServerGamemode]ss[ServerGamemode]d [ServerGamemode]pac[ServerGamemode] on th[ServerGamemode] win/[ServerGamemode]os[ServerGamemode] sc[ServerGamemode][ServerGamemode][ServerGamemode]n: skip th[ServerGamemode] [ServerGamemode][ServerGamemode]match tim[ServerGamemode][ServerGamemode] and
// sta[ServerGamemode]t th[ServerGamemode] n[ServerGamemode]xt manag[ServerGamemode]d match imm[ServerGamemode]diat[ServerGamemode][ServerGamemode]y (n[ServerGamemode]xt tick).
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]matchNow();

// Host-on[ServerGamemode]y chang[ServerGamemode]map command: [ServerGamemode][ServerGamemode][ServerGamemode]oad th[ServerGamemode] gi[ServerGamemode][ServerGamemode]n map [ServerGamemode]i[ServerGamemode][ServerGamemode] on th[ServerGamemode] n[ServerGamemode]xt tick.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Gam[ServerGamemode]mod[ServerGamemode]R[ServerGamemode]q[ServerGamemode][ServerGamemode]stMapChang[ServerGamemode](const std::st[ServerGamemode]ing& mapId);
std::st[ServerGamemode]ing s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Acti[ServerGamemode][ServerGamemode]T[ServerGamemode]amList();
boo[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]q[ServerGamemode][ServerGamemode]stT[ServerGamemode]amChang[ServerGamemode]([ServerGamemode]int32_t p[ServerGamemode]ay[ServerGamemode][ServerGamemode]Id, int [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dT[ServerGamemode]am,
                             [ServerGamemode]OCKET sock,
                             std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                             [ServerGamemode]int32_t tick, [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t,
                             std::st[ServerGamemode]ing& m[ServerGamemode]ssag[ServerGamemode]);
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]R[ServerGamemode]spawnA[ServerGamemode][ServerGamemode]Acto[ServerGamemode]s([ServerGamemode]OCKET sock,
                            std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                            std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                            [ServerGamemode]int32_t tick, [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);

// [ServerGamemode]ta[ServerGamemode]ts th[ServerGamemode] sha[ServerGamemode][ServerGamemode]d comm[ServerGamemode]nity map [ServerGamemode][ServerGamemode]ntim[ServerGamemode] witho[ServerGamemode]t [ServerGamemode]nab[ServerGamemode]ing match sco[ServerGamemode]ing.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityMap[ServerGamemode]ta[ServerGamemode]t(const std::[ServerGamemode][ServerGamemode]cto[ServerGamemode]<std::st[ServerGamemode]ing>& mapPoo[ServerGamemode],
                             const std::st[ServerGamemode]ing& mapId,
                             boo[ServerGamemode] a[ServerGamemode]toRotation,
                             [ServerGamemode]int32_t [ServerGamemode]otationMin[ServerGamemode]t[ServerGamemode]s,
                             int w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId);
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode]tMod[ServerGamemode](const std::st[ServerGamemode]ing& mod[ServerGamemode]Id);
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode][ServerGamemode]tW[ServerGamemode]apon[ServerGamemode][ServerGamemode]t(int w[ServerGamemode]apon[ServerGamemode][ServerGamemode]tId);
boo[ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponA[ServerGamemode][ServerGamemode]ow[ServerGamemode]d(const std::st[ServerGamemode]ing& w[ServerGamemode]aponId);
int s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponNati[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]ot(int [ServerGamemode]ogica[ServerGamemode][ServerGamemode][ServerGamemode]ot);
int s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nityW[ServerGamemode]aponLogica[ServerGamemode][ServerGamemode][ServerGamemode]ot(const std::st[ServerGamemode]ing& w[ServerGamemode]aponId);
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Comm[ServerGamemode]nity[ServerGamemode]ta[ServerGamemode]tMatch(boo[ServerGamemode] skipInt[ServerGamemode][ServerGamemode]mission = fa[ServerGamemode]s[ServerGamemode],
                               const std::st[ServerGamemode]ing& [ServerGamemode][ServerGamemode]q[ServerGamemode][ServerGamemode]st[ServerGamemode]dMod[ServerGamemode] = {});

// ── Bomb Tag s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] tick ──────────────────────────────────────────────
// Ca[ServerGamemode][ServerGamemode][ServerGamemode]d [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]y s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode] tick wh[ServerGamemode]n matchMod[ServerGamemode] == "bombtag".
// Hand[ServerGamemode][ServerGamemode]s bomb tim[ServerGamemode][ServerGamemode] co[ServerGamemode]ntdown, physica[ServerGamemode] contact [ServerGamemode]a[ServerGamemode]idation with [ServerGamemode][ServerGamemode]wind,
// bomb t[ServerGamemode]ansf[ServerGamemode][ServerGamemode], [ServerGamemode]xp[ServerGamemode]osion, sh[ServerGamemode]ff[ServerGamemode][ServerGamemode]-bag ho[ServerGamemode]d[ServerGamemode][ServerGamemode] s[ServerGamemode][ServerGamemode][ServerGamemode]ction, and stat[ServerGamemode] [ServerGamemode][ServerGamemode]p[ServerGamemode]ication.
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTagTick([ServerGamemode]OCKET sock,
                       std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]P[ServerGamemode]ay[ServerGamemode][ServerGamemode]>& p[ServerGamemode]ay[ServerGamemode][ServerGamemode]s,
                       H[ServerGamemode]ad[ServerGamemode][ServerGamemode]ssWo[ServerGamemode][ServerGamemode]d& wo[ServerGamemode][ServerGamemode]d,
                       std::[ServerGamemode]no[ServerGamemode]d[ServerGamemode][ServerGamemode][ServerGamemode]d_map<[ServerGamemode]int32_t, [ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]Npc>& npcs,
                       Npc[ServerGamemode]yst[ServerGamemode]m& npc[ServerGamemode]yst[ServerGamemode]m,
                       [ServerGamemode]int32_t tick,
                       [ServerGamemode]int64_t& tota[ServerGamemode]Pack[ServerGamemode]tsO[ServerGamemode]t);

// ── Bomb Tag match sta[ServerGamemode]t ──────────────────────────────────────────────
// T[ServerGamemode]ansitions comm[ServerGamemode]nity mod[ServerGamemode] into bomb tag. Initia[ServerGamemode]iz[ServerGamemode]s sh[ServerGamemode]ff[ServerGamemode][ServerGamemode] bag,
// s[ServerGamemode][ServerGamemode][ServerGamemode]cts fi[ServerGamemode]st bomb ho[ServerGamemode]d[ServerGamemode][ServerGamemode], s[ServerGamemode]ts tim[ServerGamemode][ServerGamemode].
[ServerGamemode]oid s[ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode][ServerGamemode]BombTag[ServerGamemode]ta[ServerGamemode]tMatch(boo[ServerGamemode] skipInt[ServerGamemode][ServerGamemode]mission = fa[ServerGamemode]s[ServerGamemode]);

} // nam[ServerGamemode]spac[ServerGamemode] MimitaN[ServerGamemode]t
