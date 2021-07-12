/*=====================================================================
PerlinNoise.cpp
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "PerlinNoise.h"


#include "GridNoise.h"
#include "Voronoi.h"
#include "../maths/mathstypes.h"
#include "../maths/vec2.h"
#include "../maths/Vec4i.h"
#include "../indigo/globals.h"
//#include "../indigo/HaltonSampler.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../maths/PCG32.h"
#include "../maths/GeometrySampling.h"
#include <fstream>


/*

See http://mrl.nyu.edu/~perlin/noise/
also http://www.cs.utah.edu/~aek/research/noise.pdf

*/


// Explicit template instantiation
template float PerlinNoise::FBM2(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::ridgedFBM(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::voronoiFBM(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::multifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);
template float PerlinNoise::ridgedMultifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);
template float PerlinNoise::voronoiMultifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);


const static uint8 permutation[] = { 151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};


static const Vec4f new_gradients[256] = {Vec4f(-1.3319215f, 0.18818918f, -0.43654323f, 0.f), Vec4f(0.5770185f, 0.003576953f, -1.2911377f, 0.f), Vec4f(-0.23113091f, -0.4510943f, -1.3202622f, 0.f), Vec4f(0.31901723f, -0.3718079f, 1.3266449f, 0.f), Vec4f(0.1942332f, 0.49124575f, -1.3118502f, 0.f), Vec4f(0.49572006f, 0.40368846f, -1.2614663f, 0.f), Vec4f(0.6416344f, 0.72831976f, -1.028521f, 0.f), Vec4f(-0.80989045f, -1.1129818f, -0.32457474f, 0.f), Vec4f(0.8084535f, -1.1348674f, -0.24182343f, 0.f), Vec4f(-1.0782652f, 0.7045635f, -0.58389586f, 0.f), Vec4f(-1.3886043f, 0.013263114f, 0.26758587f, 0.f), Vec4f(0.8916826f, -1.0365491f, -0.3612037f, 0.f), Vec4f(0.9601096f, -1.0365224f, 0.061733164f, 0.f), Vec4f(-0.56402713f, -0.60338444f, 1.1479548f, 0.f), Vec4f(1.2031989f, -0.43249863f, -0.60436547f, 0.f), Vec4f(1.004202f, 0.023058135f, 0.99551326f, 0.f), Vec4f(-0.28075182f, 0.6423771f, -1.2282223f, 0.f), Vec4f(-1.3189248f, -0.25452304f, -0.44232932f, 0.f), Vec4f(0.29017022f, -0.46485114f, -1.3037311f, 0.f), Vec4f(0.70859253f, -1.0330552f, 0.65627235f, 0.f), Vec4f(0.7257881f, 0.5854927f, -1.0632167f, 0.f), Vec4f(-0.9114928f, -0.2697877f, 1.0470889f, 0.f), Vec4f(0.7609878f, -1.0546814f, 0.5554678f, 0.f), Vec4f(0.9822044f, 0.6242913f, -0.80345196f, 0.f), Vec4f(-0.5698886f, -0.73164046f, -1.0676746f, 0.f), Vec4f(-1.1314874f, 0.65609664f, 0.5378415f, 0.f), Vec4f(0.78251034f, 1.1370933f, 0.30772787f, 0.f), Vec4f(-1.3636216f, 0.048129156f, 0.3717788f, 0.f), Vec4f(0.10074063f, 1.0073676f, 0.98745227f, 0.f), Vec4f(0.9173388f, -0.16100103f, 1.0642219f, 0.f), Vec4f(-0.09226979f, 1.3622584f, -0.36842674f, 0.f), Vec4f(1.3751297f, 0.23780383f, -0.22905767f, 0.f), Vec4f(0.85508716f, -0.1960402f, -1.1092312f, 0.f), Vec4f(-0.3383413f, -1.0671881f, -0.8640802f, 0.f), Vec4f(0.18917347f, -0.78322846f, -1.1622248f, 0.f), Vec4f(0.96736836f, -0.7812386f, 0.6736948f, 0.f), Vec4f(-0.6723574f, -1.040628f, 0.68193024f, 0.f), Vec4f(-0.16447094f, -0.9615571f, 1.0238932f, 0.f), Vec4f(0.6542757f, 0.2374394f, -1.231075f, 0.f), Vec4f(-1.1911525f, 0.30062264f, 0.7005582f, 0.f), Vec4f(0.9419655f, 0.11158277f, -1.0489281f, 0.f), Vec4f(0.65754074f, -0.9484127f, -0.8174066f, 0.f), Vec4f(-0.8238074f, 1.1122955f, 0.29006878f, 0.f), Vec4f(0.51405966f, 1.2110685f, 0.51870567f, 0.f), Vec4f(1.1032765f, 0.7365982f, 0.4901063f, 0.f), Vec4f(-0.44353256f, -1.3145843f, 0.2741295f, 0.f), Vec4f(-0.59011894f, -1.2666806f, 0.21743897f, 0.f), Vec4f(-1.0469567f, -0.7388439f, 0.59832406f, 0.f), Vec4f(1.1779726f, -0.39403418f, 0.6761049f, 0.f), Vec4f(-0.9425835f, 1.0510104f, 0.08314865f, 0.f), Vec4f(-0.51554996f, -0.33405057f, -1.2738204f, 0.f), Vec4f(0.39120337f, -0.19959898f, -1.3442917f, 0.f), Vec4f(-0.74423045f, -0.031190123f, 1.2021432f, 0.f), Vec4f(-1.3937404f, -0.13111988f, 0.200736f, 0.f), Vec4f(-0.36080733f, 0.82818633f, 1.0880834f, 0.f), Vec4f(0.86244327f, 1.1207304f, 0.01244411f, 0.f), Vec4f(0.3101493f, 1.1865886f, 0.7041411f, 0.f), Vec4f(-1.0734962f, 0.14830358f, 0.9086319f, 0.f), Vec4f(0.860298f, 0.4256739f, -1.0385995f, 0.f), Vec4f(-1.3940789f, 0.2322257f, -0.051143937f, 0.f), Vec4f(-0.59854543f, 0.23719367f, 1.2591594f, 0.f), Vec4f(-0.37659824f, 1.2885867f, 0.44465464f, 0.f), Vec4f(-0.6555681f, -1.179514f, -0.42305675f, 0.f), Vec4f(1.272615f, 0.2962127f, -0.5410257f, 0.f), Vec4f(-0.97883433f, 0.9472733f, -0.3802057f, 0.f), Vec4f(-0.1317501f, -1.3472008f, 0.40950233f, 0.f), Vec4f(-1.2544218f, -0.1249954f, -0.6409386f, 0.f), Vec4f(-1.2415584f, 0.6509942f, 0.18638533f, 0.f), Vec4f(-0.09392981f, -1.3893201f, -0.24691512f, 0.f), Vec4f(-1.0829396f, 0.753836f, -0.50889367f, 0.f), Vec4f(1.2407471f, 0.57596177f, -0.35890782f, 0.f), Vec4f(1.3290399f, -0.48217532f, -0.034052968f, 0.f), Vec4f(-0.76933265f, -0.31538174f, 1.1439674f, 0.f), Vec4f(0.045455627f, 0.081125215f, 1.4111528f, 0.f), Vec4f(-1.3969661f, 0.15853731f, 0.15281208f, 0.f), Vec4f(-0.54758275f, 0.000271677f, 1.3038992f, 0.f), Vec4f(-1.3510836f, 0.17228511f, 0.3806453f, 0.f), Vec4f(0.041882966f, 0.06015747f, 1.4123125f, 0.f), Vec4f(-0.95156854f, 0.96589667f, -0.40194672f, 0.f), Vec4f(-0.20752844f, 0.25036687f, -1.3763169f, 0.f), Vec4f(0.388705f, -1.3313226f, -0.27656585f, 0.f), Vec4f(1.0709022f, -0.9144165f, -0.13042544f, 0.f), Vec4f(-0.8593103f, 0.4316217f, 1.0369612f, 0.f), Vec4f(0.75653946f, -0.8925849f, -0.79431754f, 0.f), Vec4f(-0.7049585f, -0.84273195f, -0.8904135f, 0.f), Vec4f(-0.4662356f, -1.3349999f, -0.01998671f, 0.f), Vec4f(0.85664773f, 1.078009f, -0.32256964f, 0.f), Vec4f(0.5544755f, 1.3008375f, 0.0194592f, 0.f), Vec4f(0.4124557f, 1.3526769f, -0.012069425f, 0.f), Vec4f(0.65304613f, -0.42420688f, 1.1804996f, 0.f), Vec4f(0.5382383f, 0.6867629f, -1.1129493f, 0.f), Vec4f(-1.3433348f, -0.16680439f, 0.40942374f, 0.f), Vec4f(-1.0045226f, 0.41825238f, 0.9033268f, 0.f), Vec4f(0.09651176f, 1.1686954f, 0.7904659f, 0.f), Vec4f(-0.25981152f, -0.67198056f, 1.2169389f, 0.f), Vec4f(-0.39102325f, 0.5039756f, -1.2621844f, 0.f), Vec4f(-0.6877954f, 0.28618568f, 1.2020961f, 0.f), Vec4f(0.7638297f, 0.5630094f, -1.0486107f, 0.f), Vec4f(-0.54123807f, 0.18205516f, 1.2937995f, 0.f), Vec4f(0.08171073f, 1.1287546f, 0.848078f, 0.f), Vec4f(0.6425987f, 1.0941539f, 0.62441516f, 0.f), Vec4f(0.4787746f, -0.03633249f, -1.3302085f, 0.f), Vec4f(-0.39213878f, 0.011929473f, -1.358707f, 0.f), Vec4f(0.3981531f, -1.1330636f, -0.74675363f, 0.f), Vec4f(-1.3929057f, 0.19203274f, 0.15145014f, 0.f), Vec4f(1.329253f, -0.091122076f, 0.47411317f, 0.f), Vec4f(-0.60293275f, 1.2791228f, 0.017801818f, 0.f), Vec4f(-0.14541249f, -1.0507766f, 0.93526673f, 0.f), Vec4f(-1.3668414f, -0.06995429f, -0.3561611f, 0.f), Vec4f(-0.7149158f, -0.50030607f, -1.1129191f, 0.f), Vec4f(-1.0407053f, 0.943901f, -0.1611934f, 0.f), Vec4f(1.248117f, 0.14009322f, 0.65005976f, 0.f), Vec4f(1.2179712f, 0.6980104f, 0.17125377f, 0.f), Vec4f(-0.14017655f, 1.0270689f, -0.96201867f, 0.f), Vec4f(0.5740125f, 1.2159765f, -0.4380761f, 0.f), Vec4f(-1.1362697f, -0.2261453f, 0.81101763f, 0.f), Vec4f(1.0449332f, 0.12873495f, -0.9442148f, 0.f), Vec4f(-0.24536934f, -1.2038103f, -0.7004531f, 0.f), Vec4f(-0.72138226f, 0.6635814f, 1.0194446f, 0.f), Vec4f(0.32464233f, 0.04745368f, 1.3756291f, 0.f), Vec4f(0.98546934f, -0.8546678f, 0.54625374f, 0.f), Vec4f(-0.042337663f, 1.0950723f, 0.89388156f, 0.f), Vec4f(1.0775335f, -0.1459937f, 0.90421647f, 0.f), Vec4f(-0.9340105f, 0.92445064f, -0.5225088f, 0.f), Vec4f(-0.9351953f, 1.055978f, 0.10158865f, 0.f), Vec4f(-0.01564798f, 0.9122649f, -1.0805221f, 0.f), Vec4f(0.8518605f, -0.9741098f, 0.5704767f, 0.f), Vec4f(-0.38441342f, -1.2365452f, -0.56849116f, 0.f), Vec4f(-0.55560243f, -1.3003073f, -0.022515183f, 0.f), Vec4f(-0.34903473f, -1.2020946f, -0.6581365f, 0.f), Vec4f(-1.3095003f, -0.42675096f, 0.32108033f, 0.f), Vec4f(-0.9638671f, -0.12656817f, 1.027103f, 0.f), Vec4f(-0.34470308f, 0.9087151f, -1.0273347f, 0.f), Vec4f(-0.4843973f, 1.1254089f, -0.7062677f, 0.f), Vec4f(-0.04426467f, 0.1126923f, -1.4090213f, 0.f), Vec4f(-0.28542435f, 0.966347f, 0.99232376f, 0.f), Vec4f(0.21500666f, 0.5023194f, 1.3043953f, 0.f), Vec4f(-0.57176006f, 0.24651189f, 1.2697725f, 0.f), Vec4f(-0.9856086f, 0.03937642f, 1.0134224f, 0.f), Vec4f(1.309065f, -0.44890806f, -0.29125655f, 0.f), Vec4f(0.91967624f, -0.821069f, 0.6928502f, 0.f), Vec4f(-1.158751f, -0.12152173f, 0.8015786f, 0.f), Vec4f(-1.2028059f, -0.32248068f, 0.67027175f, 0.f), Vec4f(-1.3768083f, 0.1513931f, -0.28544542f, 0.f), Vec4f(0.26956078f, 0.538069f, 1.2797729f, 0.f), Vec4f(0.7849328f, 1.0581241f, -0.5140567f, 0.f), Vec4f(-0.680672f, -1.2396287f, 0.0025777011f, 0.f), Vec4f(0.08015621f, 1.3771911f, -0.3113193f, 0.f), Vec4f(0.35223806f, -1.3271734f, -0.3384359f, 0.f), Vec4f(-0.66750777f, -1.2425766f, 0.102158904f, 0.f), Vec4f(-0.32609338f, -0.3210031f, -1.3381405f, 0.f), Vec4f(1.0418985f, -0.8438716f, -0.44980937f, 0.f), Vec4f(-0.9795901f, -0.22744654f, 0.99431944f, 0.f), Vec4f(0.5589782f, 0.8834539f, -0.952393f, 0.f), Vec4f(1.4141519f, 0.012478868f, 0.004290465f, 0.f), Vec4f(0.2678242f, -1.2859728f, -0.5239695f, 0.f), Vec4f(1.2416636f, -0.24589118f, -0.6307211f, 0.f), Vec4f(-0.45992061f, 0.3221327f, -1.2979612f, 0.f), Vec4f(-0.029019369f, -1.2202915f, 0.7141753f, 0.f), Vec4f(0.65745157f, 0.7703582f, 0.98706913f, 0.f), Vec4f(0.011175341f, -0.9506189f, -1.0469952f, 0.f), Vec4f(-1.0500653f, -0.6189625f, 0.71711093f, 0.f), Vec4f(-1.2772204f, -0.59274703f, 0.1317539f, 0.f), Vec4f(-0.6404796f, 0.7602613f, 1.005877f, 0.f), Vec4f(-0.14658406f, -1.236692f, -0.67015374f, 0.f), Vec4f(0.23816338f, -0.4076415f, -1.3330816f, 0.f), Vec4f(-0.6676676f, -0.56248343f, -1.1125792f, 0.f), Vec4f(1.2921097f, -0.14130992f, 0.5572107f, 0.f), Vec4f(0.649485f, 0.48652896f, 1.1582136f, 0.f), Vec4f(1.2084122f, -0.5096774f, -0.5291208f, 0.f), Vec4f(1.2239467f, 0.13040183f, -0.69638324f, 0.f), Vec4f(-1.2286797f, -0.14867346f, 0.6842824f, 0.f), Vec4f(1.3538346f, 0.40847257f, 0.016780935f, 0.f), Vec4f(-0.30802414f, -0.12747024f, 1.3743625f, 0.f), Vec4f(1.2767122f, 0.49862355f, 0.3483968f, 0.f), Vec4f(-0.8285904f, -1.0774056f, -0.39068526f, 0.f), Vec4f(-0.5071468f, -1.0891943f, 0.7459611f, 0.f), Vec4f(1.010092f, 0.77715206f, -0.6129834f, 0.f), Vec4f(0.25009096f, 0.59092784f, -1.2602614f, 0.f), Vec4f(-0.77582014f, -0.6051713f, -1.0158104f, 0.f), Vec4f(-1.0371056f, -0.8719929f, 0.4050193f, 0.f), Vec4f(0.1793481f, -1.1829607f, 0.7539483f, 0.f), Vec4f(0.5370853f, 0.926357f, -0.9237977f, 0.f), Vec4f(1.3791455f, 0.03770027f, -0.310703f, 0.f), Vec4f(0.5560995f, -1.2439675f, 0.3785469f, 0.f), Vec4f(-0.86937535f, -1.0767791f, 0.2910894f, 0.f), Vec4f(-0.39884034f, 0.60868686f, 1.2126116f, 0.f), Vec4f(1.064279f, -0.359362f, 0.8591677f, 0.f), Vec4f(0.8624887f, 0.41171923f, 1.0424013f, 0.f), Vec4f(1.0176522f, 0.535804f, -0.82298124f, 0.f), Vec4f(0.991693f, -0.8946885f, 0.46484154f, 0.f), Vec4f(1.3870782f, 0.22220524f, 0.163213f, 0.f), Vec4f(-1.3389932f, -0.24264172f, -0.38499594f, 0.f), Vec4f(-0.13281743f, -0.8035749f, 1.1561258f, 0.f), Vec4f(1.3099176f, -0.41582856f, -0.3334703f, 0.f), Vec4f(-1.1132622f, 0.33361655f, 0.80582094f, 0.f), Vec4f(0.7626287f, -1.1384096f, 0.34988704f, 0.f), Vec4f(0.5471144f, -0.6127026f, 1.1511999f, 0.f), Vec4f(0.9467645f, -1.0198501f, 0.25207624f, 0.f), Vec4f(0.75260985f, -1.1963552f, -0.048092924f, 0.f), Vec4f(1.3719398f, 0.13033378f, 0.31748092f, 0.f), Vec4f(0.3859089f, 1.3202142f, -0.32879952f, 0.f), Vec4f(-1.4069699f, -0.0016450356f, 0.14294356f, 0.f), Vec4f(-0.88392824f, -0.42431095f, 1.0191325f, 0.f), Vec4f(-0.36900008f, 0.34115937f, 1.3219112f, 0.f), Vec4f(-1.0257475f, -0.5248928f, 0.81995696f, 0.f), Vec4f(-0.7285196f, -1.1139015f, 0.47799852f, 0.f), Vec4f(-1.0114245f, -0.88449204f, 0.44124174f, 0.f), Vec4f(0.47637874f, -1.2418581f, -0.4804701f, 0.f), Vec4f(1.0063906f, 0.9001384f, -0.42062917f, 0.f), Vec4f(-0.4289078f, 0.88406706f, -1.0170857f, 0.f), Vec4f(1.4082073f, -0.056426633f, -0.11733809f, 0.f), Vec4f(-1.3162044f, -0.5169541f, -0.01907735f, 0.f), Vec4f(0.11610599f, 1.3809185f, 0.2821054f, 0.f), Vec4f(0.34094676f, 0.22217822f, -1.3543973f, 0.f), Vec4f(0.5484877f, 0.2553046f, -1.2782726f, 0.f), Vec4f(0.23212416f, -1.3672595f, 0.2769831f, 0.f), Vec4f(-0.91225064f, 0.036186278f, 1.0800413f, 0.f), Vec4f(1.1235197f, -0.15774307f, 0.84428716f, 0.f), Vec4f(1.3589364f, -0.37654048f, -0.10728034f, 0.f), Vec4f(-1.096192f, -0.5747241f, 0.68414587f, 0.f), Vec4f(-1.0358754f, -0.9289162f, 0.25313348f, 0.f), Vec4f(-1.0164684f, 0.32275033f, -0.9287756f, 0.f), Vec4f(0.6416031f, -1.1766571f, -0.45146802f, 0.f), Vec4f(0.54536086f, 1.2106426f, -0.48675025f, 0.f), Vec4f(-1.3865291f, 0.21208195f, -0.18043841f, 0.f), Vec4f(1.1277783f, -0.03702647f, 0.8524935f, 0.f), Vec4f(-1.2496017f, 0.45359647f, 0.48243693f, 0.f), Vec4f(-0.62443984f, 0.86208147f, 0.9310695f, 0.f), Vec4f(-0.3321425f, -1.3175389f, -0.3921384f, 0.f), Vec4f(0.46589872f, -1.2181879f, 0.5467692f, 0.f), Vec4f(-1.1123129f, 0.8452513f, -0.21979581f, 0.f), Vec4f(-0.11165382f, 1.3448056f, 0.4231209f, 0.f), Vec4f(0.30827868f, -0.18445867f, 1.3678228f, 0.f), Vec4f(-0.7075515f, -0.49100143f, -1.1217346f, 0.f), Vec4f(0.9172336f, -1.0761818f, 0.022692958f, 0.f), Vec4f(-0.9868573f, -0.082664624f, 1.0095935f, 0.f), Vec4f(0.7594778f, 0.91345555f, -0.76732796f, 0.f), Vec4f(-0.56597626f, 0.99198383f, -0.8340497f, 0.f), Vec4f(-0.07804834f, -0.29610813f, -1.3806623f, 0.f), Vec4f(0.24437276f, -1.2943006f, -0.51484746f, 0.f), Vec4f(-0.57917947f, -1.0063794f, -0.8073114f, 0.f), Vec4f(-0.0014574614f, -0.374511f, -1.3637227f, 0.f), Vec4f(1.0506802f, -0.8677176f, 0.37833506f, 0.f), Vec4f(-0.8731695f, 0.54146636f, -0.9717969f, 0.f), Vec4f(0.011706607f, 0.6228577f, -1.2696106f, 0.f), Vec4f(0.048901528f, 0.6162326f, 1.2719536f, 0.f), Vec4f(0.3428282f, 1.3481166f, 0.2550492f, 0.f), Vec4f(0.86044264f, 0.7943503f, 0.7928721f, 0.f), Vec4f(0.89869326f, 0.8977964f, 0.62154f, 0.f), Vec4f(-0.40851316f, -1.3357925f, -0.22085091f, 0.f), Vec4f(-1.3354927f, -0.3168343f, 0.34069818f, 0.f), Vec4f(-0.6963697f, -1.2226862f, 0.14180171f, 0.f), Vec4f(1.405333f, 0.15804414f, -0.0077962396f, 0.f), Vec4f(1.1895617f, 0.054590087f, -0.7628648f, 0.f), Vec4f(-0.8041028f, 0.71266925f, -0.9195222f, 0.f), };
static int masks[4] = {132, 125, 91, 250}; // For perlin 4-valued noise


// NOTE: if the random generation of new_gradients and masks changes, then we need to update the opencl data.
void PerlinNoise::buildData()
{
	PCG32 rng(1);

	const int N = 256;
	/*for(int i=0; i<N; ++i)
		p_x[i] = p_y[i] = p_z[i] = (uint8)i;

	// Permute
	for(int t=N-1; t>=0; --t)
	{
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_x[t], p_x[k]);
		}
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_y[t], p_y[k]);
		}
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_z[t], p_z[k]);
		}
	}*/
	
	// Make new_gradients
	//HaltonSampler halton;

	Vec4f write_gradients[256];
	
	for(int i=0; i<N; ++i)
	{
		//float samples[2];
		//halton.getSamples(samples, 2);

		//new_gradients[i] = GeometrySampling::unitSquareToSphere(Vec2f(samples[0], samples[1])).toVec4fVector() * std::sqrt(2.f);
		write_gradients[i] = GeometrySampling::unitSquareToSphere(Vec2f(rng.unitRandom(), rng.unitRandom())).toVec4fVector() * std::sqrt(2.f);
	}

	// Permute directions
	/*for(int t=N-1; t>=0; --t)
	{
		{
			int k = (int)(rng.unitRandom() * N);
			mySwap(new_gradients[t], new_gradients[k]);
		}
	}*/

	
	// Some code to print out the tables used, so the OpenCL implementation can use the same data
	{
		std::ofstream file("perlin_data.txt");

		// Compute a random bit mask for each result dimension
		int new_masks[4];
		for(int i=0; i<4; ++i)
			new_masks[i] = (int)rng.genrand_int32() & 0xFF;

		/*file << "\np_x = ";
		for(int i=0; i<N; ++i)
			file << toString(p_x[i]) + ", ";

		file << "\np_y = ";
		for(int i=0; i<N; ++i)
			file << toString(p_y[i]) + ", ";

		file << "\np_z = ";
		for(int i=0; i<N; ++i)
			file << toString(p_z[i]) + ", ";*/

		file << "\nfloat new_gradient_comp_vec3[768] = {";
		for(int i=0; i<N; ++i)
			file << ::floatLiteralString(write_gradients[i].x[0]) << ", " << ::floatLiteralString(write_gradients[i].x[1]) << ", " << ::floatLiteralString(write_gradients[i].x[2]) << ", ";
		file << "};\n";


		file << "\nstatic Vec4f new_gradients[256] = {";
		for(int i=0; i<N; ++i)
				file << "Vec4f(" + ::floatLiteralString(write_gradients[i].x[0]) << ", " << ::floatLiteralString(write_gradients[i].x[1]) << ", " << ::floatLiteralString(write_gradients[i].x[2]) << ", " << ::floatLiteralString(write_gradients[i].x[3]) << "), ";
		file << "};\n";
		
		file << "int masks[4] = {";
		for(int i=0; i<4; ++i)
			file << toString(new_masks[i]) + ", ";
		file << "};\n";
	}
}


inline const Vec4f fade(const Vec4f& t) 
{ 
	return t * t * t * (t * (t * Vec4f(6) - Vec4f(15)) + Vec4f(10)); 
}


/*
	Fast Perlin noise implementation using SSE
	==========================================

	for each 8 corner positions p_c with gradient vector g_c

	d_c = dot(p - p_c, g_c)

	where p_c = floored(p) + offset[c]

	(offset is offset from bottom left corner to corner c)

	so 
	d_c = dot(p - (floored(p) + offset[c]), g_c)
	    = dot(p - floored(p) - offset[c], g_c)
		= dot(fractional - offset[c], g_c)

	res = w_0*d_0 + w_1*d_1 + w_2*d_2 + ... + w_7*d_7

	where

	w_0 = (1 - u) * (1 - v) * (1 - w)
	w_1 = (    u) * (1 - v) * (1 - w)
	w_2 = (1 - u) * (    v) * (1 - w)
	...

	(w_0*d_0)_x = w_0 * d_c_x
				= w_0 * (fractional_x - offset[0]_x) * g_0_x
	(w_1*d_1)_x = w_1 * (fractional_x - offset[1]_x) * g_1_x
	(w_2*d_2)_x = w_2 * (fractional_x - offset[2]_x) * g_2_x
	(w_3*d_3)_x = w_3 * (fractional_x - offset[3]_x) * g_3_x
	...
*/


// 3-vector input, returns scalar value.
float PerlinNoise::noise(const Vec4f& point)
{
	const Vec4f floored = floor(point); // SSE 4 floor
	const Vec4i floored_i = toVec4i(floored);

	// NOTE: ANDing with 0xFF is faster in scalar code than SSE in this function.
	const int X = floored_i[0] & 0xFF;
	const int Y = floored_i[1] & 0xFF;
	const int Z = floored_i[2] & 0xFF;
	assert(X >= 0 && X < 256);
	assert(Y >= 0 && Y < 256);
	assert(Z >= 0 && Z < 256);

	// NOTE: doing the (X + 1) & 0xFF in SSE is slower than the scalar code below.
	const int hash_x  = GridNoise::p_x[X];
	const int hash_x1 = GridNoise::p_x[(X + 1) & 0xFF];
	const int hash_y  = GridNoise::p_y[Y];
	const int hash_y1 = GridNoise::p_y[(Y + 1) & 0xFF];
	const int hash_z  = GridNoise::p_z[Z];
	const int hash_z1 = GridNoise::p_z[(Z + 1) & 0xFF];


	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);

	const Vec4f gradient_i_0 = new_gradients[hash_x  ^ hash_y  ^ hash_z ];
	const Vec4f gradient_i_1 = new_gradients[hash_x1 ^ hash_y  ^ hash_z ];
	const Vec4f gradient_i_2 = new_gradients[hash_x  ^ hash_y1 ^ hash_z ];
	const Vec4f gradient_i_3 = new_gradients[hash_x1 ^ hash_y1 ^ hash_z ];
	const Vec4f gradient_i_4 = new_gradients[hash_x  ^ hash_y  ^ hash_z1];
	const Vec4f gradient_i_5 = new_gradients[hash_x1 ^ hash_y  ^ hash_z1];
	const Vec4f gradient_i_6 = new_gradients[hash_x  ^ hash_y1 ^ hash_z1];
	const Vec4f gradient_i_7 = new_gradients[hash_x1 ^ hash_y1 ^ hash_z1];


#if 0
	// Reference/naive code.  Significantly slower.
	const float u_ = uvw[0];
	const float v_ = uvw[1];
	const float w_ = uvw[2];
	return
		(1 - u_) * (1 - v_) * (1 - w_) * dot(gradient_i_0, fractional - Vec4f(0,0,0,0)) +
		(    u_) * (1 - v_) * (1 - w_) * dot(gradient_i_1, fractional - Vec4f(1,0,0,0)) +
		(1 - u_) * (    v_) * (1 - w_) * dot(gradient_i_2, fractional - Vec4f(0,1,0,0)) +
		(    u_) * (    v_) * (1 - w_) * dot(gradient_i_3, fractional - Vec4f(1,1,0,0)) +
		(1 - u_) * (1 - v_) * (    w_) * dot(gradient_i_4, fractional - Vec4f(0,0,1,0)) +
		(    u_) * (1 - v_) * (    w_) * dot(gradient_i_5, fractional - Vec4f(1,0,1,0)) +
		(1 - u_) * (    v_) * (    w_) * dot(gradient_i_6, fractional - Vec4f(0,1,1,0)) +
		(    u_) * (    v_) * (    w_) * dot(gradient_i_7, fractional - Vec4f(1,1,1,0));
#else
	const Vec4f one_uvw = Vec4f(1) - uvw;

	const Vec4f fractional_x = copyToAll<0>(fractional);
	const Vec4f fractional_y = copyToAll<1>(fractional);
	const Vec4f fractional_z = copyToAll<2>(fractional);

	/*
	w_0 = (1 - u) * (1 - v) * (1 - w)
	w_1 = (    u) * (1 - v) * (1 - w)
	w_2 = (1 - u) * (    v) * (1 - w)
	w_3 = (    u) * (    v) * (1 - w)
	w_4 = (1 - u) * (1 - v) * (    w)
	w_5 = (    u) * (1 - v) * (    w)
	w_6 = (1 - u) * (    v) * (    w)
	w_7 = (    u) * (    v) * (    w)

	Note that the u, v, factors repeat for w0-w3 and w4-w7.  So put those factors in uv.
	They can then be multipled by 1-w or w.
	*/

	const Vec4f a = unpacklo(uvw, one_uvw); // [u, 1-u, v, 1-v]
	const Vec4f uvec = shuffle<1, 0, 1, 0>(a, a); // [1 - u, u, 1 - u, u]
	const Vec4f vvec = shuffle<3, 3, 2, 2>(a, a); // [1 - v, 1 - v, v, v]
	const Vec4f uv = uvec * vvec;


	/*
	Offsets (cube corners) are
	Vec4f(0,0,0,0),
	Vec4f(1,0,0,0),
	Vec4f(0,1,0,0),
	Vec4f(1,1,0,0),
	Vec4f(0,0,1,0),
	Vec4f(1,0,1,0),
	Vec4f(0,1,1,0),
	Vec4f(1,1,1,0)

	frac_offset_0_x = fractional_x - 0
	frac_offset_1_x = fractional_x - 1
	frac_offset_2_x = fractional_x - 0
	frac_offset_3_x = fractional_x - 1
	...
	*/

	const Vec4f frac_offset_x = fractional_x - Vec4f(0, 1, 0, 1);
	const Vec4f frac_offset_y = fractional_y - Vec4f(0, 0, 1, 1);
	
	//==================== Corners 0 - 3 ======================
	// Tranpose so that gradients_x is the x coordinates of gradient_i_0, gradient_i_1, etc..
	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;

	transpose(gradient_i_0, gradient_i_1, gradient_i_2, gradient_i_3,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y = frac_offset_y * gradients_y;
	const Vec4f weighted_dot_z = fractional_z  * gradients_z;

	const Vec4f sum_weighted_dot = (weighted_dot_x + weighted_dot_y + weighted_dot_z) * copyToAll<2>(one_uvw);

	//==================== Corners 4 - 7 ======================
	transpose(gradient_i_4, gradient_i_5, gradient_i_6, gradient_i_7,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x_2 = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y_2 = frac_offset_y * gradients_y;
	const Vec4f weighted_dot_z_2 = (fractional_z - Vec4f(1)) * gradients_z;
		
	const Vec4f sum_weighted_dot_2 = (weighted_dot_x_2 + weighted_dot_y_2 + weighted_dot_z_2) * copyToAll<2>(uvw);

	const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
	
	// NOTE: serial sum is faster than SSE horizontalSum
	return sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
#endif
}


// 2-vector input, returns scalar value
float PerlinNoise::noise(float x, float y)
{
	const Vec4f point(x, y, 0, 0);
	const Vec4f floored = floor(point); // SSE 4 floor.
	const Vec4i floored_i = toVec4i(floored);

	const int X = floored_i.x[0] & 0xFF;
	const int Y = floored_i.x[1] & 0xFF;
	
	const int hash_x  = GridNoise::p_x[X];
	const int hash_x1 = GridNoise::p_x[(X + 1) & 0xFF];
	const int hash_y  = GridNoise::p_y[Y];
	const int hash_y1 = GridNoise::p_y[(Y + 1) & 0xFF];

	
	const Vec4f fractional = point - floored; // Fractional coords in noise space
	const Vec4f uvw = fade(fractional); // Interpolation weights

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);

	const Vec4f one_uvw = Vec4f(1) - uvw;
	const Vec4f a = unpacklo(uvw, one_uvw); // [u, 1-u, v, 1-v]
	const Vec4f uvec = shuffle<1, 0, 1, 0>(a, a); // [1 - u, u, 1 - u, u]
	const Vec4f vvec = shuffle<3, 3, 2, 2>(a, a); // [1 - v, 1 - v, v, v]
	const Vec4f uv = uvec * vvec;

	const Vec4f gradient_i_0 = new_gradients[hash_x  ^ hash_y ];
	const Vec4f gradient_i_1 = new_gradients[hash_x1 ^ hash_y ];
	const Vec4f gradient_i_2 = new_gradients[hash_x  ^ hash_y1];
	const Vec4f gradient_i_3 = new_gradients[hash_x1 ^ hash_y1];

	/*
	Offsets (cube corners) are
	Vec4f(0,0,0,0),
	Vec4f(1,0,0,0),
	Vec4f(0,1,0,0),
	Vec4f(1,1,0,0),
	*/

	const Vec4f frac_offset_x = fractional_x - Vec4f(0, 1, 0, 1);
	const Vec4f frac_offset_y = fractional_y - Vec4f(0, 0, 1, 1);
	
	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;

	transpose(gradient_i_0, gradient_i_1, gradient_i_2, gradient_i_3,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y = frac_offset_y * gradients_y;

	const Vec4f sum = (weighted_dot_x + weighted_dot_y) * uv;
	
	return sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
}


float PerlinNoise::periodicNoise(float x, float y, int period) // Period with period in x and y directions.  Period must be a power of two.
{
	assert(Maths::isPowerOfTwo(period));
	const int mask = period - 1;

	const Vec4f point(x, y, 0, 0);
	const Vec4f floored = floor(point); // SSE 4 floor.
	const Vec4i floored_i = toVec4i(floored);

	const int X = floored_i.x[0] & mask;
	const int Y = floored_i.x[1] & mask;
	
	const int hash_x  = GridNoise::p_x[X];
	const int hash_x1 = GridNoise::p_x[(X + 1) & mask];
	const int hash_y  = GridNoise::p_y[Y];
	const int hash_y1 = GridNoise::p_y[(Y + 1) & mask];

	
	const Vec4f fractional = point - floored; // Fractional coords in noise space
	const Vec4f uvw = fade(fractional); // Interpolation weights

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);

	const Vec4f one_uvw = Vec4f(1) - uvw;
	const Vec4f a = unpacklo(uvw, one_uvw); // [u, 1-u, v, 1-v]
	const Vec4f uvec = shuffle<1, 0, 1, 0>(a, a); // [1 - u, u, 1 - u, u]
	const Vec4f vvec = shuffle<3, 3, 2, 2>(a, a); // [1 - v, 1 - v, v, v]
	const Vec4f uv = uvec * vvec;

	const Vec4f gradient_i_0 = new_gradients[hash_x  ^ hash_y ];
	const Vec4f gradient_i_1 = new_gradients[hash_x1 ^ hash_y ];
	const Vec4f gradient_i_2 = new_gradients[hash_x  ^ hash_y1];
	const Vec4f gradient_i_3 = new_gradients[hash_x1 ^ hash_y1];

	/*
	Offsets (cube corners) are
	Vec4f(0,0,0,0),
	Vec4f(1,0,0,0),
	Vec4f(0,1,0,0),
	Vec4f(1,1,0,0),
	*/

	const Vec4f frac_offset_x = fractional_x - Vec4f(0, 1, 0, 1);
	const Vec4f frac_offset_y = fractional_y - Vec4f(0, 0, 1, 1);
	
	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;

	transpose(gradient_i_0, gradient_i_1, gradient_i_2, gradient_i_3,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y = frac_offset_y * gradients_y;

	const Vec4f sum = (weighted_dot_x + weighted_dot_y) * uv;
	
	return sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
}


/*
Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.  3-vector input.
*/
const Vec4f PerlinNoise::noise4Valued(const Vec4f& point)
{
	const Vec4f floored = floor(point); // SSE 4 floor.

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored);// &Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0] & 0xFF;
	const int Y = floored_i.x[1] & 0xFF;
	const int Z = floored_i.x[2] & 0xFF;
	
	const int hash_x  = GridNoise::p_x[X];
	const int hash_x1 = GridNoise::p_x[(X + 1) & 0xFF];
	const int hash_y  = GridNoise::p_y[Y];
	const int hash_y1 = GridNoise::p_y[(Y + 1) & 0xFF];
	const int hash_z  = GridNoise::p_z[Z];
	const int hash_z1 = GridNoise::p_z[(Z + 1) & 0xFF];



	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);
	const Vec4f one_uvw = Vec4f(1) - uvw;

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);
	const Vec4f fractional_z(fractional.x[2]);

	const Vec4f a = unpacklo(uvw, one_uvw); // [u, 1-u, v, 1-v]
	const Vec4f uvec = shuffle<1, 0, 1, 0>(a, a); // [1 - u, u, 1 - u, u]
	const Vec4f vvec = shuffle<3, 3, 2, 2>(a, a); // [1 - v, 1 - v, v, v]
	const Vec4f uv = uvec * vvec;


	Vec4f res;
	const Vec4f offsets_x(0, 1, 0, 1);
	const Vec4f offsets_y(0, 0, 1, 1);
	const Vec4f rel_offset_x = fractional_x - offsets_x;
	const Vec4f rel_offset_y = fractional_y - offsets_y;
	const Vec4f rel_offset_z = fractional_z;
	const Vec4f rel_offset_z2 = fractional_z - Vec4f(1,1,1,1);

	const int h0 = hash_x  ^ hash_y  ^ hash_z ;
	const int h1 = hash_x1 ^ hash_y  ^ hash_z ;
	const int h2 = hash_x  ^ hash_y1 ^ hash_z ;
	const int h3 = hash_x1 ^ hash_y1 ^ hash_z ;
	const int h4 = hash_x  ^ hash_y  ^ hash_z1;
	const int h5 = hash_x1 ^ hash_y  ^ hash_z1;
	const int h6 = hash_x  ^ hash_y1 ^ hash_z1;
	const int h7 = hash_x1 ^ hash_y1 ^ hash_z1;

	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;
	{
		const int mask = masks[0];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[0] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}
	{
		const int mask = masks[1];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[1] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[2];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[2] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[3];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[3] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	return res;
}


/*
Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.
*/
const Vec4f PerlinNoise::noise4Valued(float x, float y)
{
	const Vec4f point(x, y, 0, 0);
	const Vec4f floored = floor(point); // SSE 4 floor.

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored);// &Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0] & 0xFF;
	const int Y = floored_i.x[1] & 0xFF;
	
	const int hash_x  = GridNoise::p_x[X];
	const int hash_x1 = GridNoise::p_x[(X + 1) & 0xFF];
	const int hash_y  = GridNoise::p_y[Y];
	const int hash_y1 = GridNoise::p_y[(Y + 1) & 0xFF];

	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);
	const Vec4f one_uvw = Vec4f(1) - uvw;

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);

	const Vec4f a = unpacklo(uvw, one_uvw); // [u, 1-u, v, 1-v]
	const Vec4f uvec = shuffle<1, 0, 1, 0>(a, a); // [1 - u, u, 1 - u, u]
	const Vec4f vvec = shuffle<3, 3, 2, 2>(a, a); // [1 - v, 1 - v, v, v]
	const Vec4f uv = uvec * vvec;


	Vec4f res;
	const Vec4f offsets_x(0, 1, 0, 1);
	const Vec4f offsets_y(0, 0, 1, 1);
	const Vec4f rel_offset_x = fractional_x - offsets_x;
	const Vec4f rel_offset_y = fractional_y - offsets_y;

	const int h0 = hash_x  ^ hash_y ;
	const int h1 = hash_x1 ^ hash_y ;
	const int h2 = hash_x  ^ hash_y1;
	const int h3 = hash_x1 ^ hash_y1;

	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;
	{
		const int mask = masks[0];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[0] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}
	{
		const int mask = masks[1];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[1] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[2];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[2] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[3];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[3] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	return res;
}


float PerlinNoise::FBM(const Vec4f& point, unsigned int num_octaves)
{
	float sum = 0;
	float scale = 1;
	float weight = 1;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += weight * PerlinNoise::noise(point * scale); // Use SSE 4 in PerlinNoise::noiseImpl()
		scale *= (float)1.99;
		weight *= (float)0.5;
	}
	return sum;
}


float PerlinNoise::FBM(float x, float y, unsigned int num_octaves)
{
	float sum = 0;
	float scale = 1;
	float weight = 1;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += weight * PerlinNoise::noise(x * scale, y * scale); // Use SSE 4 in PerlinNoise::noiseImpl()
		scale *= (float)1.99;
		weight *= (float)0.5;
	}
	return sum;
}


float PerlinNoise::periodicFBM(float x, float y, unsigned int num_octaves, int period)
{
	float sum = 0;
	float scale = 1;
	float weight = 1;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += weight * PerlinNoise::periodicNoise(x * scale, y * scale, period); // Use SSE 4 in PerlinNoise::noiseImpl()
		scale *= (float)2;
		weight *= (float)0.5;

		period *= 2;
	}
	return sum;
}


const Vec4f PerlinNoise::FBM4Valued(const Vec4f& point, unsigned int num_octaves)
{
	Vec4f sum(0);
	float scale = 1;
	float weight = 1;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += PerlinNoise::noise4Valued(point * scale) * weight; // Use SSE 4 in PerlinNoise::noiseImpl()
		scale *= (float)1.99;
		weight *= (float)0.5;
	}
	return sum;
}


const Vec4f PerlinNoise::FBM4Valued(float x, float y, unsigned int num_octaves)
{
	Vec4f sum(0);
	float scale = 1;
	float weight = 1;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += PerlinNoise::noise4Valued(x * scale, y * scale) * weight; // Use SSE 4 in PerlinNoise::noiseImpl()
		scale *= (float)1.99;
		weight *= (float)0.5;
	}
	return sum;
}


//================================== Noise basis functions =============================================


struct PerlinBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		return PerlinNoise::noise(p) * 0.5f + 0.5f;
	}
};


struct PerlinBasisNoise
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return PerlinNoise::noise(p);
	}
};


struct RidgedBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return 1 - std::fabs(PerlinNoise::noise(p));
	}
};


struct RidgedBasisNoise
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return 0.5f - std::fabs(PerlinNoise::noise(p));
	}
};


struct VoronoiBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		Vec4f closest_p;
		float dist;
		Voronoi::evaluate3d(p, 1.0f, closest_p, dist);
		return dist;
	}
};


//================================== New (more general) FBM =============================================


/*
Adapted from 'Texturing and Modelling, a Procedural Approach'
Page 437, fBm()

Computing the weight for each frequency, from the book:
w = f^-H
f = l^n
where n = octave, f = freq, l = lacunarity
so
w = (l^n)^-H
  = l^(-nH)
  = (l^-H)^n

*/


template <class Real, class BasisFunction>
Real genericFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves, BasisFunction basis_func)
{
	Real sum = 0;
	Real freq = 1;
	Real weight = 1;
	Real w_factor = std::pow(lacunarity, -H);

	const Vec4f v = p - Vec4f(0,0,0,1);

	for(int i=0; i<(int)octaves; ++i)
	{
		sum += weight * basis_func.eval(Vec4f(0,0,0,1) + v * freq);
		freq *= lacunarity;
		weight *= w_factor;
	}

	// Do remaining octaves
	Real d = octaves - (int)octaves;
	if(d > 0)
		sum += d * weight * basis_func.eval(Vec4f(0,0,0,1) + v * freq);
	
	return sum;
}


template <class Real>
Real PerlinNoise::FBM2(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, PerlinBasisNoise>(p, H, lacunarity, octaves, PerlinBasisNoise());
}


template <class Real>
Real PerlinNoise::ridgedFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, RidgedBasisNoise>(p, H, lacunarity, octaves, RidgedBasisNoise());
}


template <class Real>
Real PerlinNoise::voronoiFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, VoronoiBasisNoise01>(p, H, lacunarity, octaves, VoronoiBasisNoise01());
}


//================================== Multifractal =============================================


template <class Real, class BasisFunction>
Real genericMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset, BasisFunction basis_func)
{
	Real value = 0;
	Real freq = 1;
	Real weight = 1;
	Real w_factor = std::pow(lacunarity, -H);

	const Vec4f v = p - Vec4f(0,0,0,1);

	value += weight * (basis_func.eval(Vec4f(0,0,0,1) + v * freq) + offset);

	for(int i=1; i<(int)octaves; ++i)
	{
		value += myMax<Real>(0, value) * weight * (basis_func.eval(Vec4f(0,0,0,1) + v * freq) + offset);
		
		freq *= lacunarity;
		weight *= w_factor;
	}
	return value;
}


template <class Real>
Real PerlinNoise::multifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, PerlinBasisNoise01>(p, H, lacunarity, octaves, offset, PerlinBasisNoise01());
}


template <class Real>
Real PerlinNoise::ridgedMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, RidgedBasisNoise01>(p, H, lacunarity, octaves, offset, RidgedBasisNoise01());
}


template <class Real>
Real PerlinNoise::voronoiMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, VoronoiBasisNoise01>(p, H, lacunarity, octaves, offset, VoronoiBasisNoise01());
}
