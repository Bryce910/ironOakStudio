#ifndef VALWOOD_WORLD_PLAYER_INVENTORY_H
#define VALWOOD_WORLD_PLAYER_INVENTORY_H
#include "world_weapon_loadout.h"
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
// Inventory used by the playable opening and arena. Pure transactions are
// staged before the host replaces its entire checkpoint atomically.
namespace world_player_inventory {
enum class Item : unsigned {None,Sword,Bow,SwordShield,Axe,Staff,TravelArmor,Mail,Helmet,Gloves,Boots,Bread,Apple,Potion,Satchel,WatchLetter,Count};
enum class Kind {Weapon,Armor,Food,Quest};
enum class Slot : unsigned {Chest,Head,Hands,Feet,Count};
struct Definition {const char *name,*description,*rarity;Kind kind;unsigned limit;float weight;float protection;int slot;};
inline constexpr std::array<Definition,unsigned(Item::Count)> definitions={{
 {"Empty","","",Kind::Quest,0,0,0,-1},
 {"Bram's longsword","A balanced two-handed blade. Three-hit directional combos.","COMMON",Kind::Weapon,1,3,0,-1},
 {"Practice bow","Hold and release to shoot. Available in arena practice.","COMMON",Kind::Weapon,1,1.5f,0,-1},
 {"Sword and oak shield","One-handed sword with a shield. Lower blocking stamina cost.","COMMON",Kind::Weapon,1,5,0,-1},
 {"Woodland great axe","A heavy steel edge and long ash haft. Slower, powerful cuts.","COMMON",Kind::Weapon,1,4,0,-1},
 {"Traveler's quarterstaff","Long reach, fast blunt strikes and inexpensive combos.","COMMON",Kind::Weapon,1,2,0,-1},
 {"Worn travel armor","Leather traveling harness over your clothes. Reduces incoming damage by 4%.","COMMON",Kind::Armor,1,4,.04f,0},
 {"Roadwarden mail","Recovered armor. Reduces incoming damage by 10%.","UNCOMMON",Kind::Armor,1,7,.10f,0},
 {"Iron helmet","A serviceable helm. Reduces incoming damage by 2%.","COMMON",Kind::Armor,1,1.5f,.02f,1},
 {"Leather gloves","A firm grip. Reduces incoming damage by 1%.","COMMON",Kind::Armor,1,.4f,.01f,2},
 {"Travel boots","Sturdy boots. Reduces incoming damage by 1%.","COMMON",Kind::Armor,1,.8f,.01f,3},
 {"Bread","Eat to restore 30 health. Cannot be wasted at full health.","COMMON",Kind::Food,20,.25f,0,-1},
 {"Orchard apple","Eat to restore 35 stamina. Cannot be wasted at full stamina.","COMMON",Kind::Food,20,.15f,0,-1},
 {"Healing draught","Drink to restore 60 health. Cannot be wasted at full health.","UNCOMMON",Kind::Food,20,.2f,0,-1},
 {"Rowan's provision satchel","Return these supplies to Rowan at the arrival camp.","QUEST ITEM",Kind::Quest,1,0,0,-1},
 {"Letter of introduction","Rowan's introduction to the people of Seldonne.","QUEST ITEM",Kind::Quest,1,0,0,-1}
}};
constexpr unsigned loan_receipt=1,loot_receipt=2,satchel_receipt=4,reward_receipt=8,battlefield_receipt=16,all_receipts=31,capacity=24;
struct State {
 std::array<unsigned,unsigned(Item::Count)> counts={};
 Item weapon=Item::None;std::array<Item,unsigned(Slot::Count)> armor={};
 unsigned receipts=0;std::uint64_t spent=0;
};
inline bool known(Item i){return unsigned(i)>0&&unsigned(i)<unsigned(Item::Count);}
inline const Definition &definition(Item i){return definitions[known(i)?unsigned(i):0];}
inline unsigned count(const State &s,Item i){return known(i)?s.counts[unsigned(i)]:0;}
inline unsigned occupied(const State &s){unsigned n=0;for(unsigned i=1;i<s.counts.size();++i)if(s.counts[i]&&definitions[i].kind!=Kind::Quest)++n;return n;}
inline bool valid(const State &s){
 if(s.counts[0]||s.receipts>all_receipts||s.spent>100000000000ull||occupied(s)>capacity)return false;
 if((s.receipts&battlefield_receipt)&&count(s,Item::Sword)!=1)return false;
 for(unsigned i=1;i<s.counts.size();++i)if(s.counts[i]>definitions[i].limit)return false;
 if(s.weapon!=Item::None&&(!known(s.weapon)||definition(s.weapon).kind!=Kind::Weapon||!count(s,s.weapon)))return false;
 for(unsigned k=0;k<s.armor.size();++k){auto i=s.armor[k];if(i!=Item::None&&(!known(i)||definition(i).kind!=Kind::Armor||definition(i).slot!=int(k)||!count(s,i)))return false;}
 return true;
}
inline bool add(State &s,Item i,unsigned n=1){if(!known(i)||!n||n>definition(i).limit||count(s,i)>definition(i).limit-n)return false;State next=s;next.counts[unsigned(i)]+=n;if(!valid(next))return false;s=next;return true;}
inline Item weapon_item(world_weapon_loadout::Weapon w){switch(w){case world_weapon_loadout::Weapon::Sword:return Item::Sword;case world_weapon_loadout::Weapon::Bow:return Item::Bow;case world_weapon_loadout::Weapon::SwordShield:return Item::SwordShield;case world_weapon_loadout::Weapon::GreatAxe:return Item::Axe;case world_weapon_loadout::Weapon::Staff:return Item::Staff;}return Item::None;}
inline world_weapon_loadout::Weapon weapon_type(Item i){switch(i){case Item::Bow:return world_weapon_loadout::Weapon::Bow;case Item::SwordShield:return world_weapon_loadout::Weapon::SwordShield;case Item::Axe:return world_weapon_loadout::Weapon::GreatAxe;case Item::Staff:return world_weapon_loadout::Weapon::Staff;default:return world_weapon_loadout::Weapon::Sword;}}
inline bool owns(const State &s,world_weapon_loadout::Weapon w){return world_weapon_loadout::valid(w)&&count(s,weapon_item(w));}
inline float protection(const State &s){float n=0;for(auto i:s.armor)n+=definition(i).protection;return std::clamp(n,0.f,.30f);}
inline unsigned appearance(const State &s){return (s.armor[0]==Item::Mail?4:s.armor[0]==Item::TravelArmor?2:1)|(s.armor[1]==Item::Helmet?16:8)|(s.armor[2]==Item::Gloves?64:32)|(s.armor[3]==Item::Boots?256:128);}
inline float weight(const State &s){float n=0;for(unsigned i=1;i<s.counts.size();++i)n+=s.counts[i]*definitions[i].weight;return n;}
inline bool equipped(const State &s,Item i){if(i==Item::None)return false;if(s.weapon==i)return true;return std::find(s.armor.begin(),s.armor.end(),i)!=s.armor.end();}
inline bool equip(State &s,Item i,std::string &message){
 if(!valid(s)||!count(s,i)){message="You do not own that item.";return false;}
 const auto &d=definition(i);
 if(d.kind==Kind::Weapon)s.weapon=i;
 else if(d.kind==Kind::Armor)s.armor[unsigned(d.slot)]=i;
 else {message="This item cannot be equipped.";return false;}
 message=std::string(d.name)+" equipped.";return true;
}
inline bool unequip(State &s,Item i,std::string &message){
 if(!valid(s)||!equipped(s,i)||definition(i).kind!=Kind::Armor){message="Select an equipped armor item.";return false;}
 s.armor[unsigned(definition(i).slot)]=Item::None;message=std::string(definition(i).name)+" moved to backpack.";return true;
}
inline bool use(State &s,Item i,float &health,float &stamina,std::string &message){
 if(!valid(s)||!count(s,i)||!std::isfinite(health)||!std::isfinite(stamina)||health<=0){message="This item cannot be used now.";return false;}
 const bool healing=i==Item::Bread||i==Item::Potion;
 if(!healing&&i!=Item::Apple){message="Select food or a healing draught.";return false;}
 float &value=healing?health:stamina;if(value>=100){message=healing?"Health is already full. Item kept.":"Stamina is already full. Item kept.";return false;}
 const float restored=std::min(100.f-value,i==Item::Bread?30.f:i==Item::Potion?60.f:35.f);value+=restored;--s.counts[unsigned(i)];message=std::string(definition(i).name)+": +"+std::to_string(int(restored))+(healing?" health.":" stamina.");return true;
}
inline std::uint64_t purse(const State &s,std::uint64_t earned){return s.spent<=earned?earned-s.spent:0;}
inline unsigned price(Item i){return i==Item::Bread?3:i==Item::Apple?2:i==Item::Potion?5:i==Item::Mail?45:i==Item::Helmet?20:i==Item::Gloves?6:i==Item::Boots?8:0;}
inline bool buy(State &s,Item i,std::uint64_t earned,std::string &message){
 const auto cost=price(i);if(!valid(s)||!cost||s.spent>earned||purse(s,earned)<cost){message="Not enough coin.";return false;}
 auto next=s;if(!add(next,i)){message=definition(i).kind==Kind::Armor?"You already own this armor.":"That stack is full.";return false;}next.spent+=cost;if(!valid(next)){message="This purchase could not be saved.";return false;}s=next;message=std::string(definition(i).name)+" bought for "+std::to_string(cost)+" copper.";return true;
}
inline State loan_kit(bool practice=false){
 State s;for(auto i:{Item::Sword,Item::SwordShield,Item::Axe,Item::Staff,Item::TravelArmor,Item::Gloves,Item::Boots})add(s,i);
 if(practice)add(s,Item::Bow);
 s.weapon=Item::Sword;s.armor={Item::TravelArmor,Item::None,Item::Gloves,Item::Boots};s.receipts=loan_receipt;return s;
}
// The battlefield supplies one stolen blade, never a complete practice kit.
// Receipt and item commit together; replaying contact/load cannot refill gear.
inline bool grant_battlefield_sword(State &s){
 if(!valid(s))return false;
 if(s.receipts&battlefield_receipt)return true;
 auto n=s;if(!count(n,Item::Sword)&&!add(n,Item::Sword))return false;
 n.weapon=Item::Sword;n.receipts|=battlefield_receipt;if(!valid(n))return false;
 s=n;return true;
}
inline bool grant_loan(State &s){
 if(!valid(s))return false;
 if(s.receipts&loan_receipt)return true;
 if(s.receipts&battlefield_receipt){s.receipts|=loan_receipt;return true;}
 State n=loan_kit();n.spent=s.spent;for(unsigned i=1;i<s.counts.size();++i)n.counts[i]=std::max(n.counts[i],s.counts[i]);n.receipts|=s.receipts;if(!valid(n))return false;
 s=n;return true;
}
inline bool loot_raider(State &s,std::string &message){
 if(s.receipts&loot_receipt){message="Already searched. No items remain.";return false;}
 auto n=s;for(auto i:{Item::Mail,Item::Helmet,Item::Potion})if(!(definition(i).kind==Kind::Armor&&count(n,i))&&!add(n,i)){message="Make room in your backpack first.";return false;}
 if(!add(n,Item::Apple,2)){message="Your fruit stack is full.";return false;}n.receipts|=loot_receipt;s=n;message="Loot: roadwarden mail, iron helmet, healing draught and 2 apples.";return true;
}
// Reconcile once-only story grants with authoritative quest state. Never refill
// consumed items on re-entry, load, retry or repeated dialogue.
inline bool reconcile(State &s,bool loaned,bool supplies,bool rewarded){
 auto n=s;if(loaned&&!grant_loan(n))return false;
 if(supplies&&!(n.receipts&satchel_receipt)){if(!add(n,Item::Satchel))return false;n.receipts|=satchel_receipt;}
 if(rewarded&&!(n.receipts&reward_receipt)){n.counts[unsigned(Item::Satchel)]=0;if(!add(n,Item::WatchLetter)||!add(n,Item::Bread))return false;n.receipts|=reward_receipt;}
 if(!valid(n))return false;
 s=n;return true;
}
inline bool matches_story(const State &s,bool loaned,bool supplies,bool rewarded){
 return valid(s)&&(!(s.receipts&battlefield_receipt)||count(s,Item::Sword)==1)&&bool(s.receipts&loan_receipt)==loaned&&bool(s.receipts&satchel_receipt)==supplies&&bool(s.receipts&reward_receipt)==rewarded&&
 (!(s.receipts&loot_receipt)||loaned)&&!count(s,Item::Bow)&&count(s,Item::Satchel)==unsigned(supplies&&!rewarded)&&count(s,Item::WatchLetter)==unsigned(rewarded);
}
inline std::string encode(const State &s){
 if(!valid(s))return {};
 std::ostringstream o;o<<"VALWOOD_INVENTORY 1\nitems";for(auto n:s.counts)o<<' '<<n;o<<"\nequipment "<<unsigned(s.weapon);for(auto i:s.armor)o<<' '<<unsigned(i);o<<"\nreceipts "<<s.receipts<<"\nspent "<<s.spent<<"\nend\n";return o.str();
}
inline bool decode(const std::string &bytes,State &out,std::string &error){
 if(bytes.empty()||bytes.size()>2048){error="Inventory record exceeds bounds.";return false;}
 State s;std::istringstream in(bytes);std::string tag;unsigned version=0,w=0;
 if(!(in>>tag>>version)||tag!="VALWOOD_INVENTORY"||version!=1||!(in>>tag)||tag!="items"){error="Invalid inventory header.";return false;}
 for(auto &n:s.counts)if(!(in>>n)){error="Incomplete item stacks.";return false;}
 if(!(in>>tag>>w)||tag!="equipment"){error="Missing inventory equipment.";return false;}s.weapon=Item(w);
 for(auto &i:s.armor){if(!(in>>w)){error="Incomplete armor equipment.";return false;}i=Item(w);}
 if(!(in>>tag>>s.receipts)||tag!="receipts"||!(in>>tag>>s.spent)||tag!="spent"||!(in>>tag)||tag!="end"||!valid(s)||encode(s)!=bytes){error="Inventory could not be verified. Existing save preserved.";return false;}
 out=s;error.clear();return true;
}
}
#endif
