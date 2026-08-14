#pragma once
#include <cstdint>

// Animation pack data (name + per-slot asset ids) supplied by the user.
// Each slot = { group child under Animate, animation child under group, asset id }.

namespace Cheat {
namespace Features {
namespace CharMods {

struct AnimSlotDef
{
	const char* group;
	const char* anim;
	std::uint64_t id;
};

struct AnimPackDef
{
	const char* name;
	const AnimSlotDef* slots;
	int count;
};

inline const AnimSlotDef k2014[] = {
	{"idle","Animation1",79230074465081ull},
	{"idle","Animation2",108128190588873ull},
	{"walk","WalkAnim",129176008736462ull},
	{"run","RunAnim",109490732869533ull},
	{"jump","JumpAnim",82537739274282ull},
	{"fall","FallAnim",115218010785890ull},
	{"climb","ClimbAnim",86309925713925ull},
	{"swimidle","SwimIdle",92452074550508ull},
	{"swim","Swim",125096636102405ull},
};

inline const AnimSlotDef kAdidasAura[] = {
	{"idle","Animation1",110211186840347ull},
	{"idle","Animation2",114191137265065ull},
	{"walk","WalkAnim",83842218823011ull},
	{"run","RunAnim",118320322718866ull},
	{"jump","JumpAnim",109996626521204ull},
	{"fall","FallAnim",95603166884636ull},
	{"climb","ClimbAnim",97824616490448ull},
	{"swimidle","SwimIdle",94922130551805ull},
	{"swim","Swim",134530128383903ull},
};

inline const AnimSlotDef kAdidasCommunity[] = {
	{"idle","Animation1",122257458498464ull},
	{"idle","Animation2",102357151005774ull},
	{"walk","WalkAnim",122150855457006ull},
	{"run","RunAnim",82598234841035ull},
	{"jump","JumpAnim",75290611992385ull},
	{"fall","FallAnim",98600215928904ull},
	{"climb","ClimbAnim",88763136693023ull},
	{"swimidle","SwimIdle",109346520324160ull},
	{"swim","Swim",133308483266208ull},
};

inline const AnimSlotDef kAdidasSports[] = {
	{"idle","Animation1",18537376492ull},
	{"idle","Animation2",18537371272ull},
	{"walk","WalkAnim",18537392113ull},
	{"run","RunAnim",18537384940ull},
	{"jump","JumpAnim",18537380791ull},
	{"fall","FallAnim",18537367238ull},
	{"climb","ClimbAnim",18537363391ull},
	{"swimidle","SwimIdle",18537387180ull},
	{"swim","Swim",18537389531ull},
};

inline const AnimSlotDef kAmazonUnboxed[] = {
	{"idle","Animation1",98281136301627ull},
	{"idle","Animation2",138183121662404ull},
	{"walk","WalkAnim",90478085024465ull},
	{"run","RunAnim",134824450619865ull},
	{"jump","JumpAnim",121454505477205ull},
	{"fall","FallAnim",94788218468396ull},
	{"climb","ClimbAnim",121145883950231ull},
	{"swimidle","SwimIdle",129126268464847ull},
	{"swim","Swim",105962919001086ull},
};

inline const AnimSlotDef kBillieEilish[] = {
	{"idle","Animation1",102934602884410ull},
	{"idle","Animation2",92151291669373ull},
	{"walk","WalkAnim",81877886552514ull},
	{"run","RunAnim",100920560634123ull},
	{"jump","JumpAnim",117602630922781ull},
	{"fall","FallAnim",81072141180299ull},
	{"climb","ClimbAnim",117873469361430ull},
	{"swimidle","SwimIdle",78535650384589ull},
	{"swim","Swim",121824746242877ull},
};

inline const AnimSlotDef kBold[] = {
	{"idle","Animation1",16738333868ull},
	{"idle","Animation2",16738334710ull},
	{"walk","WalkAnim",16738340646ull},
	{"run","RunAnim",16738337225ull},
	{"jump","JumpAnim",16738336650ull},
	{"fall","FallAnim",16738333171ull},
	{"climb","ClimbAnim",16738332169ull},
	{"swimidle","SwimIdle",16738339817ull},
	{"swim","Swim",16738339158ull},
};

inline const AnimSlotDef kBorock[] = {
	{"idle","Animation1",3293641938ull},
	{"idle","Animation2",3293642554ull},
};

inline const AnimSlotDef kBubbly[] = {
	{"idle","Animation1",10921054344ull},
	{"idle","Animation2",10921055107ull},
	{"walk","WalkAnim",10980888364ull},
	{"run","RunAnim",10921057244ull},
	{"jump","JumpAnim",10921062673ull},
	{"fall","FallAnim",10921061530ull},
	{"climb","ClimbAnim",10921053544ull},
	{"swimidle","SwimIdle",10922582160ull},
	{"swim","Swim",10921063569ull},
};

inline const AnimSlotDef kCartoony[] = {
	{"idle","Animation1",10921071918ull},
	{"idle","Animation2",10921072875ull},
	{"walk","WalkAnim",10921082452ull},
	{"run","RunAnim",10921076136ull},
	{"jump","JumpAnim",10921078135ull},
	{"fall","FallAnim",10921077030ull},
	{"climb","ClimbAnim",10921070953ull},
	{"swimidle","SwimIdle",10921081059ull},
	{"swim","Swim",10921079380ull},
};

inline const AnimSlotDef kCatwalkGlam[] = {
	{"idle","Animation1",133806214992291ull},
	{"idle","Animation2",94970088341563ull},
	{"walk","WalkAnim",109168724482748ull},
	{"run","RunAnim",81024476153754ull},
	{"jump","JumpAnim",116936326516985ull},
	{"fall","FallAnim",92294537340807ull},
	{"climb","ClimbAnim",119377220967554ull},
	{"swimidle","SwimIdle",98854111361360ull},
	{"swim","Swim",134591743181628ull},
};

inline const AnimSlotDef kCowboy[] = {
	{"idle","Animation1",1014390418ull},
	{"idle","Animation2",1014398616ull},
	{"walk","WalkAnim",1014421541ull},
	{"run","RunAnim",1014401683ull},
	{"jump","JumpAnim",1014394726ull},
	{"fall","FallAnim",1014384571ull},
	{"climb","ClimbAnim",1014380606ull},
	{"swimidle","SwimIdle",1014411816ull},
	{"swim","Swim",1014406523ull},
};

inline const AnimSlotDef kDefault[] = {
	{"idle","Animation1",507766388ull},
	{"idle","Animation2",507766666ull},
	{"walk","WalkAnim",913402848ull},
	{"run","RunAnim",913376220ull},
	{"jump","JumpAnim",507765000ull},
	{"fall","FallAnim",507767968ull},
	{"climb","ClimbAnim",507765644ull},
	{"swimidle","SwimIdle",913389285ull},
	{"swim","Swim",913384386ull},
};

inline const AnimSlotDef kDefaultRetarget[] = {
	{"idle","Animation1",101210804817434ull},
	{"idle","Animation2",95884606664820ull},
	{"walk","WalkAnim",115825677624788ull},
	{"run","RunAnim",102294264237491ull},
	{"jump","JumpAnim",117150377950987ull},
	{"fall","FallAnim",110205622518029ull},
	{"climb","ClimbAnim",110967007211137ull},
	{"swimidle","SwimIdle",70573966249880ull},
	{"swim","Swim",135296543654863ull},
};
inline const AnimSlotDef kElder[] = {
	{"idle","Animation1",10921101664ull},
	{"idle","Animation2",10921102574ull},
	{"walk","WalkAnim",10921111375ull},
	{"run","RunAnim",10921104374ull},
	{"jump","JumpAnim",10921107367ull},
	{"fall","FallAnim",10921105765ull},
	{"climb","ClimbAnim",10921100400ull},
	{"swimidle","SwimIdle",10921110146ull},
	{"swim","Swim",10921108971ull},
};

inline const AnimSlotDef kFallback[] = {
	{"idle","Animation1",507766388ull},
	{"idle","Animation2",507766951ull},
	{"walk","WalkAnim",507777826ull},
	{"run","RunAnim",507767714ull},
	{"jump","JumpAnim",507765000ull},
	{"fall","FallAnim",507767968ull},
	{"climb","ClimbAnim",10921100400ull},
	{"swimidle","SwimIdle",507785072ull},
	{"swim","Swim",507784897ull},
};

inline const AnimSlotDef kGlowMotion[] = {
	{"idle","Animation1",137764781910579ull},
	{"idle","Animation2",96439737641086ull},
	{"walk","WalkAnim",85809016093530ull},
	{"run","RunAnim",101925097435036ull},
	{"jump","JumpAnim",74159004634379ull},
	{"fall","FallAnim",98070939608691ull},
	{"climb","ClimbAnim",108236155509584ull},
	{"swimidle","SwimIdle",112946194103503ull},
	{"swim","Swim",83003487432457ull},
};

inline const AnimSlotDef kKATSEYE[] = {
	{"idle","Animation1",108187809145790ull},
	{"idle","Animation2",72329200359275ull},
	{"walk","WalkAnim",99182913548783ull},
	{"run","RunAnim",73117360545482ull},
	{"jump","JumpAnim",103632305262747ull},
	{"fall","FallAnim",127802717128367ull},
	{"climb","ClimbAnim",106213237973858ull},
	{"swimidle","SwimIdle",138619485942849ull},
	{"swim","Swim",134148268480210ull},
};

inline const AnimSlotDef kKnight[] = {
	{"idle","Animation1",10921117521ull},
	{"idle","Animation2",10921118894ull},
	{"walk","WalkAnim",10921121197ull},
	{"run","RunAnim",10921121197ull},
	{"jump","JumpAnim",10921123517ull},
	{"fall","FallAnim",10921116196ull},
	{"climb","ClimbAnim",106213237973858ull},
	{"swimidle","SwimIdle",10921125935ull},
	{"swim","Swim",10921125160ull},
};

inline const AnimSlotDef kLevitation[] = {
	{"idle","Animation1",10921132962ull},
	{"idle","Animation2",10921133721ull},
	{"walk","WalkAnim",10921140719ull},
	{"run","RunAnim",10921135644ull},
	{"jump","JumpAnim",10921137402ull},
	{"fall","FallAnim",10921136539ull},
	{"climb","ClimbAnim",10921132092ull},
	{"swimidle","SwimIdle",10921139478ull},
	{"swim","Swim",10921138209ull},
};
inline const AnimSlotDef kMrToilet[] = {
	{"idle","Animation1",4417977954ull},
	{"idle","Animation2",4417978624ull},
	{"run","RunAnim",4417979645ull},
};

inline const AnimSlotDef kNFL[] = {
	{"idle","Animation1",92080889861410ull},
	{"idle","Animation2",74451233229259ull},
	{"walk","WalkAnim",110358958299415ull},
	{"run","RunAnim",117333533048078ull},
	{"jump","JumpAnim",119846112151352ull},
	{"fall","FallAnim",129773241321032ull},
	{"climb","ClimbAnim",134630013742019ull},
	{"swimidle","SwimIdle",79090109939093ull},
	{"swim","Swim",132697394189921ull},
};

inline const AnimSlotDef kNinja[] = {
	{"idle","Animation1",10921155160ull},
	{"idle","Animation2",10921155867ull},
	{"walk","WalkAnim",10921162768ull},
	{"run","RunAnim",10921157929ull},
	{"jump","JumpAnim",10921160088ull},
	{"fall","FallAnim",10921159222ull},
	{"climb","ClimbAnim",10921154678ull},
	{"swimidle","SwimIdle",10922757002ull},
	{"swim","Swim",10921161002ull},
};

inline const AnimSlotDef kNoBoundaries[] = {
	{"idle","Animation1",18747067405ull},
	{"idle","Animation2",18747063918ull},
	{"walk","WalkAnim",18747074203ull},
	{"run","RunAnim",18747070484ull},
	{"jump","JumpAnim",18747069148ull},
	{"fall","FallAnim",18747062535ull},
	{"climb","ClimbAnim",18747060903ull},
	{"swimidle","SwimIdle",18747071682ull},
	{"swim","Swim",18747073181ull},
};

inline const AnimSlotDef kOldschool[] = {
	{"idle","Animation1",10921230744ull},
	{"idle","Animation2",10921232093ull},
	{"walk","WalkAnim",10921244891ull},
	{"run","RunAnim",10921240218ull},
	{"jump","JumpAnim",10921242013ull},
	{"fall","FallAnim",10921241244ull},
	{"climb","ClimbAnim",10921229866ull},
	{"swimidle","SwimIdle",10921244018ull},
	{"swim","Swim",10921243048ull},
};

inline const AnimSlotDef kPatrol[] = {
	{"idle","Animation1",1149612882ull},
	{"idle","Animation2",1150842221ull},
	{"walk","WalkAnim",1151231493ull},
	{"run","RunAnim",1150967949ull},
	{"jump","JumpAnim",1150944216ull},
	{"fall","FallAnim",1148863382ull},
	{"climb","ClimbAnim",1148811837ull},
	{"swimidle","SwimIdle",1151221899ull},
	{"swim","Swim",1151204998ull},
};

inline const AnimSlotDef kPirate[] = {
	{"idle","Animation1",750781874ull},
	{"idle","Animation2",750782770ull},
	{"walk","WalkAnim",750785693ull},
	{"run","RunAnim",750783738ull},
	{"jump","JumpAnim",750782230ull},
	{"fall","FallAnim",750780242ull},
	{"climb","ClimbAnim",750779899ull},
	{"swimidle","SwimIdle",750785176ull},
	{"swim","Swim",750784579ull},
};
inline const AnimSlotDef kPopstar[] = {
	{"idle","Animation1",1212900985ull},
	{"idle","Animation2",1212954651ull},
	{"walk","WalkAnim",1212980338ull},
	{"run","RunAnim",1212980348ull},
	{"jump","JumpAnim",1212954642ull},
	{"fall","FallAnim",1212900995ull},
	{"climb","ClimbAnim",1213044953ull},
	{"swimidle","SwimIdle",1212998578ull},
	{"swim","Swim",1212852603ull},
};

inline const AnimSlotDef kPrincess[] = {
	{"idle","Animation1",941003647ull},
	{"idle","Animation2",941013098ull},
	{"walk","WalkAnim",941028902ull},
	{"run","RunAnim",941015281ull},
	{"jump","JumpAnim",941008832ull},
	{"fall","FallAnim",941000007ull},
	{"climb","ClimbAnim",940996062ull},
	{"swimidle","SwimIdle",941025398ull},
	{"swim","Swim",941018893ull},
};

inline const AnimSlotDef kR6[] = {
	{"idle","Animation2",110291203484752ull},
	{"idle","Animation1",131260802088656ull},
	{"walk","WalkAnim",81275475859584ull},
	{"run","RunAnim",86501760984532ull},
	{"jump","JumpAnim",109853826438411ull},
	{"fall","FallAnim",110640511139183ull},
	{"climb","ClimbAnim",81319122881778ull},
	{"swimidle","SwimIdle",114255213893452ull},
	{"swim","Swim",75467827271831ull},
};

inline const AnimSlotDef kRealistic[] = {
	{"idle","Animation2",17173014241ull},
	{"idle","Animation1",17172918855ull},
	{"walk","WalkAnim",11600249883ull},
	{"run","RunAnim",11600211410ull},
	{"jump","JumpAnim",11600210487ull},
	{"fall","FallAnim",11600206437ull},
	{"climb","ClimbAnim",11600205519ull},
	{"swimidle","SwimIdle",11600213505ull},
	{"swim","Swim",11600212676ull},
};

inline const AnimSlotDef kRobot[] = {
	{"idle","Animation1",10921248039ull},
	{"idle","Animation2",10921248831ull},
	{"walk","WalkAnim",10921255446ull},
	{"run","RunAnim",10921250460ull},
	{"jump","JumpAnim",10921252123ull},
	{"fall","FallAnim",10921251156ull},
	{"climb","ClimbAnim",10921247141ull},
	{"swimidle","SwimIdle",10921253767ull},
	{"swim","Swim",10921253142ull},
};

inline const AnimSlotDef kRthro[] = {
	{"idle","Animation2",10921258489ull},
	{"idle","Animation1",10921259953ull},
	{"walk","WalkAnim",10921269718ull},
	{"run","RunAnim",10921261968ull},
	{"jump","JumpAnim",10921263860ull},
	{"fall","FallAnim",10921262864ull},
	{"climb","ClimbAnim",10921257536ull},
	{"swimidle","SwimIdle",10921265698ull},
	{"swim","Swim",10921264784ull},
};

inline const AnimSlotDef kRthroHeavy[] = {
	{"run","RunAnim",3236836670ull},
};
inline const AnimSlotDef kSneaky[] = {
	{"idle","Animation2",1132473842ull},
	{"idle","Animation1",1132477671ull},
	{"walk","WalkAnim",1132510133ull},
	{"run","RunAnim",1132494274ull},
	{"jump","JumpAnim",1132489853ull},
	{"fall","FallAnim",1132469004ull},
	{"climb","ClimbAnim",1132461372ull},
	{"swimidle","SwimIdle",11132506407ull},
	{"swim","Swim",1132500520ull},
};

inline const AnimSlotDef kSuperhero[] = {
	{"idle","Animation1",10921288909ull},
	{"idle","Animation2",10921290167ull},
	{"walk","RunAnim",10921298616ull},
	{"run","RunAnim",10921291831ull},
	{"jump","JumpAnim",10921294559ull},
	{"fall","FallAnim",10921293373ull},
	{"climb","ClimbAnim",10921286911ull},
	{"swimidle","SwimIdle",10921297391ull},
	{"swim","Swim",10921295495ull},
};

inline const AnimSlotDef kSteven[] = {
	{"idle","Animation1",105514975554898ull},
	{"idle","Animation2",80873549026270ull},
	{"walk","WalkAnim",73935678087300ull},
	{"run","RunAnim",86028285109536ull},
	{"jump","JumpAnim",128193557442109ull},
	{"fall","FallAnim",105844133916862ull},
	{"climb","ClimbAnim",119233460706851ull},
	{"swimidle","SwimIdle",85254089135641ull},
	{"swim","Swim",105514975554898ull},
};

inline const AnimSlotDef kStylish[] = {
	{"idle","Animation1",10921272275ull},
	{"idle","Animation2",10921273958ull},
	{"walk","WalkAnim",10921283326ull},
	{"run","RunAnim",10921276116ull},
	{"jump","JumpAnim",10921279832ull},
	{"fall","FallAnim",10921278648ull},
	{"climb","ClimbAnim",10921271391ull},
	{"swimidle","SwimIdle",10921281964ull},
	{"swim","Swim",10921281000ull},
};

inline const AnimSlotDef kStylizedFemale[] = {
	{"idle","Animation1",4708191566ull},
	{"idle","Animation2",4708192150ull},
	{"walk","WalkAnim",4708193840ull},
	{"run","RunAnim",4708192705ull},
	{"jump","JumpAnim",4708188025ull},
	{"fall","FallAnim",4708186162ull},
	{"climb","ClimbAnim",4708184253ull},
	{"swimidle","SwimIdle",4708190607ull},
	{"swim","Swim",4708189360ull},
};

inline const AnimSlotDef kToy[] = {
	{"idle","Animation1",10921301576ull},
	{"idle","Animation2",10921302207ull},
	{"walk","WalkAnim",10921312010ull},
	{"run","RunAnim",10921306285ull},
	{"jump","JumpAnim",10921308158ull},
	{"fall","FallAnim",10921307241ull},
	{"climb","ClimbAnim",10921300839ull},
	{"swimidle","SwimIdle",10921310341ull},
	{"swim","Swim",10921309319ull},
};

inline const AnimSlotDef kUdZal[] = {
	{"idle","Animation1",3303162274ull},
	{"idle","Animation2",3303162549ull},
};
inline const AnimSlotDef kVampire[] = {
	{"idle","Animation1",10921315373ull},
	{"idle","Animation2",10921316709ull},
	{"walk","WalkAnim",10921326949ull},
	{"run","RunAnim",10921320299ull},
	{"jump","JumpAnim",10921322186ull},
	{"fall","FallAnim",10921321317ull},
	{"climb","ClimbAnim",10921314188ull},
	{"swimidle","SwimIdle",10921325443ull},
	{"swim","Swim",10921324408ull},
};

inline const AnimSlotDef kWerewolf[] = {
	{"idle","Animation1",10921330408ull},
	{"idle","Animation2",10921333667ull},
	{"walk","WalkAnim",10921342074ull},
	{"run","RunAnim",10921336997ull},
	{"jump","JumpAnim",1083218792ull},
	{"fall","FallAnim",10921337907ull},
	{"climb","ClimbAnim",10921329322ull},
	{"swimidle","SwimIdle",10921341319ull},
	{"swim","Swim",10921340419ull},
};

inline const AnimSlotDef kWickedDTS[] = {
	{"idle","Animation1",92849173543269ull},
	{"idle","Animation2",132238900951109ull},
	{"walk","WalkAnim",73718308412641ull},
	{"run","RunAnim",135515454877967ull},
	{"jump","JumpAnim",78508480717326ull},
	{"fall","FallAnim",78147885297412ull},
	{"climb","ClimbAnim",129447497744818ull},
	{"swimidle","SwimIdle",129183123083281ull},
	{"swim","Swim",110657013921774ull},
};

inline const AnimSlotDef kWickedPopular[] = {
	{"idle","Animation1",118832222982049ull},
	{"idle","Animation2",76049494037641ull},
	{"walk","WalkAnim",92072849924640ull},
	{"run","RunAnim",72301599441680ull},
	{"jump","JumpAnim",104325245285198ull},
	{"fall","FallAnim",121152442762481ull},
	{"climb","ClimbAnim",131326830509784ull},
	{"swimidle","SwimIdle",113199415118199ull},
	{"swim","Swim",99384245425157ull},
};

inline const AnimSlotDef kZombie[] = {
	{"idle","Animation1",10921344533ull},
	{"idle","Animation2",10921345304ull},
	{"walk","WalkAnim",10921355261ull},
	{"run","RunAnim",616163682ull},
	{"jump","JumpAnim",10921351278ull},
	{"fall","FallAnim",10921350320ull},
	{"climb","ClimbAnim",10921343576ull},
	{"swimidle","SwimIdle",10921353442ull},
	{"swim","Swim",10921352344ull},
};

inline const AnimPackDef kAnimPacks[] = {
	{ "2014", k2014, (int)(sizeof(k2014)/sizeof(k2014[0])) },
	{ "Adidas Aura", kAdidasAura, (int)(sizeof(kAdidasAura)/sizeof(kAdidasAura[0])) },
	{ "Adidas Community", kAdidasCommunity, (int)(sizeof(kAdidasCommunity)/sizeof(kAdidasCommunity[0])) },
	{ "Adidas Sports", kAdidasSports, (int)(sizeof(kAdidasSports)/sizeof(kAdidasSports[0])) },
	{ "Amazon Unboxed", kAmazonUnboxed, (int)(sizeof(kAmazonUnboxed)/sizeof(kAmazonUnboxed[0])) },
	{ "Billie Eilish", kBillieEilish, (int)(sizeof(kBillieEilish)/sizeof(kBillieEilish[0])) },
	{ "Bold", kBold, (int)(sizeof(kBold)/sizeof(kBold[0])) },
	{ "Borock", kBorock, (int)(sizeof(kBorock)/sizeof(kBorock[0])) },
	{ "Bubbly", kBubbly, (int)(sizeof(kBubbly)/sizeof(kBubbly[0])) },
	{ "Cartoony", kCartoony, (int)(sizeof(kCartoony)/sizeof(kCartoony[0])) },
	{ "Catwalk Glam", kCatwalkGlam, (int)(sizeof(kCatwalkGlam)/sizeof(kCatwalkGlam[0])) },
	{ "Cowboy", kCowboy, (int)(sizeof(kCowboy)/sizeof(kCowboy[0])) },
	{ "Default", kDefault, (int)(sizeof(kDefault)/sizeof(kDefault[0])) },
	{ "Default Retarget", kDefaultRetarget, (int)(sizeof(kDefaultRetarget)/sizeof(kDefaultRetarget[0])) },
	{ "Elder", kElder, (int)(sizeof(kElder)/sizeof(kElder[0])) },
	{ "Fallback", kFallback, (int)(sizeof(kFallback)/sizeof(kFallback[0])) },
	{ "Glow Motion", kGlowMotion, (int)(sizeof(kGlowMotion)/sizeof(kGlowMotion[0])) },
	{ "KATSEYE", kKATSEYE, (int)(sizeof(kKATSEYE)/sizeof(kKATSEYE[0])) },
	{ "Knight", kKnight, (int)(sizeof(kKnight)/sizeof(kKnight[0])) },
	{ "Levitation", kLevitation, (int)(sizeof(kLevitation)/sizeof(kLevitation[0])) },
	{ "Mr. Toilet", kMrToilet, (int)(sizeof(kMrToilet)/sizeof(kMrToilet[0])) },
	{ "NFL", kNFL, (int)(sizeof(kNFL)/sizeof(kNFL[0])) },
	{ "Ninja", kNinja, (int)(sizeof(kNinja)/sizeof(kNinja[0])) },
	{ "No Boundaries", kNoBoundaries, (int)(sizeof(kNoBoundaries)/sizeof(kNoBoundaries[0])) },
	{ "Oldschool", kOldschool, (int)(sizeof(kOldschool)/sizeof(kOldschool[0])) },
	{ "Patrol", kPatrol, (int)(sizeof(kPatrol)/sizeof(kPatrol[0])) },
	{ "Pirate", kPirate, (int)(sizeof(kPirate)/sizeof(kPirate[0])) },
	{ "Popstar", kPopstar, (int)(sizeof(kPopstar)/sizeof(kPopstar[0])) },
	{ "Princess", kPrincess, (int)(sizeof(kPrincess)/sizeof(kPrincess[0])) },
	{ "R6", kR6, (int)(sizeof(kR6)/sizeof(kR6[0])) },
	{ "Realistic", kRealistic, (int)(sizeof(kRealistic)/sizeof(kRealistic[0])) },
	{ "Robot", kRobot, (int)(sizeof(kRobot)/sizeof(kRobot[0])) },
	{ "Rthro", kRthro, (int)(sizeof(kRthro)/sizeof(kRthro[0])) },
	{ "Rthro Heavy", kRthroHeavy, (int)(sizeof(kRthroHeavy)/sizeof(kRthroHeavy[0])) },
	{ "Sneaky", kSneaky, (int)(sizeof(kSneaky)/sizeof(kSneaky[0])) },
	{ "Superhero", kSuperhero, (int)(sizeof(kSuperhero)/sizeof(kSuperhero[0])) },
	{ "Steven", kSteven, (int)(sizeof(kSteven)/sizeof(kSteven[0])) },
	{ "Stylish", kStylish, (int)(sizeof(kStylish)/sizeof(kStylish[0])) },
	{ "Stylized Female", kStylizedFemale, (int)(sizeof(kStylizedFemale)/sizeof(kStylizedFemale[0])) },
	{ "Toy", kToy, (int)(sizeof(kToy)/sizeof(kToy[0])) },
	{ "Ud-zal", kUdZal, (int)(sizeof(kUdZal)/sizeof(kUdZal[0])) },
	{ "Vampire", kVampire, (int)(sizeof(kVampire)/sizeof(kVampire[0])) },
	{ "Werewolf", kWerewolf, (int)(sizeof(kWerewolf)/sizeof(kWerewolf[0])) },
	{ "Wicked DTS", kWickedDTS, (int)(sizeof(kWickedDTS)/sizeof(kWickedDTS[0])) },
	{ "Wicked Popular", kWickedPopular, (int)(sizeof(kWickedPopular)/sizeof(kWickedPopular[0])) },
	{ "Zombie", kZombie, (int)(sizeof(kZombie)/sizeof(kZombie[0])) },
};

inline const int kAnimPackCount = (int)(sizeof(kAnimPacks) / sizeof(kAnimPacks[0]));

} // namespace CharMods
} // namespace Features
} // namespace Cheat
