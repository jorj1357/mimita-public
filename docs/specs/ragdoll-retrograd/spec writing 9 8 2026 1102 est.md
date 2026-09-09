9 8 2026 1145 am jorj - todo finsish the docs, i dont want to  do the docs right now bc this seems like , input/output not  right? it will ofc take time to implement but thats ok. i want to focus more on the arcade fun aspects first

9 8 2026 1104am jorj 
# order
1. destructible world first
2. then phsical objects that i can stand on and move and inherit veocity
3. then blood and viscera and gore and bullet holes spec 
4. then ragdoll after these work bc shooting holes in player body should do damage and blood etc 

# destructible world

1. things in blender that are marked smoehow as destructible=true, not sure how to do but it would be like specific walls and specific parts of the map, specific parts themselves can be destroyed. this also relates to players plrs shoudl be destructibel as well like a shotgun shot to  the head should  make a hole and then they die as a result of idk, too much body mass lost or blood lost or brain doesnt work bc brkoen etc idk how to explain in arcade terms tho 

2. the phsical ideal result is: depends on the weapon and material. so revovler for example, should make an actual hole there, based on force and force is just the angle and speed of hit, so we should prob make it so weapons can shoot projectiles like hitscan weapons have new mode for projectiles each weapon but not implemented yet, need to write that into C:\mimita-priv-v8\docs\specs\weapons\weapons.md here 

3. yes instnat client sided predcition of the hole instnat instnat as soon as u can , then the sever just replicates that later after confimred good

4. yes, just substract spehre subtract box subtract capsule etc. being as purre math as we can is good, as efficeint as possible as we can get. i just want to ensure we donthave like, for exmaple if we make a minigun, it should not destruct  the entire wall bc its a  cube with 1 face made of 2 triangles meaning the whole triangle destructed bc it was hit , just mkae a little circle ofdestruction, and we prob can do stuff like  randomized ish desutrciton so its not just perfct cirlce 

5. visual debris is simple and just client sided , in the same step that we send a authoritive world destruct event to clients, each client themselves simulats their own visual debris, shoudl be hot reloadable as well  C:\mimita-priv-v8\config\destructible-world.jsonhere for now  thats defined or should be defined there

6. this is good i want the like, get total mass of the whole idk, we need a quick way to get all suppotring touching instances and thier points of contact, and if a mass is over a limit like if u are 10kg and ur a big concrete slab and u onl ahve 1 support in the center then ur fine but if that support is gone then some kinda math  shows ok u have no more support contacts and cant stand there anmore, not sure here tho 

7. gets awakened, like if floor gone, but table was on floor, crate on table, then table awakes as soon as its supoort is gone, in teh same exact step that we remove  the worldl geomerty, adn then the crate on the table as a result of that also falls bc the thing its sitting on is falling/has no support 

8. something in blender for now, then over time we move to being just in the engine, like map creation not a big deal in the game its not super advanced , need to figure out a better gui method tho but idk. for now, just in blender. in blender, destructible objects , im not sure, should have a better method than just naming it "destructible" , if theres a tag sstem in blender then id do that , or collections but even those are kinda iffy so idk. itll be in belnder tho . that json thing is good i like that, and over time we should ahve like, settings like this material/this part has density of 1000, or something,. it shouldl all use the metirc ssytem, every single part of the game

9. yes the engine owns the animations. so blender can put like an ideal animation, and then in the game its converted to idk  a json? or something? then it uses thoes coordinates as  the kefarmes for the animation, for exmple a player, but those are wish positions/coordinates, the engine does the  moving to the wish positions, and that stuff shudl be hot reloadabl like how muhc strenth it has to move to that wish pos/rotation, etc, but also applied to moving objects like, i wnt to have a map with  infinitely moving platforms like a factory of some sort, u can hang onthe platforms with ragdoll mode, u can explode the platforms and its fine bc new ones spawn in later anyways, blodo and viscera works with them fine,  theres specific animation/movement modes like some objects should be set as indestructible like floors and support things . i like that elevator json example 

10. i want to over time include general relativity and light speed and stuff. so over time, we should be able to  fight on an actual earth real life sized map, like the entire earth is your map, and you have huge city sized attacks that can explode entire buildings with accurate debris, destroy the ground underneath u, etc. might relate to  C:\mimita-priv-v8\docs\specs\procedural-infinite-world\procedural-infinite-world.md th eprocedural world. bc if we can get  a seed system to work, then all clients have the same seed, and just generate it client insatntly/as fast as the client can geenrate it, then send that genration info to the server in a queue so the server does not lag, and new genreations are queud or throttled or just made super duper efficnet, we can have people flying at like 500 m/s 1000 m/s even higher like 999,999 m/s thru a infinitely generated procedural city world, with anime style  fights happening like, imapcting a huge tower at a super fast speed liek 1000 m/s shoudl treat the plauer as a phsics object, and it should make a crater or destruction in the building as an emergent result of hitting it so hard, then u can just dash off of it and fight the person who threw u there in the first place etc etc

explain the summary of the current plan ur thinking  and then aks more questions to get it more detailed further and better 

## destuct world 2

1. thats good the tags thing 
2. that is amazing i want that , actual real life phsics here  in engine 
3. that is good, i want that 
4. good 
5. good 
6. i do want chunks tho over time, a concrete wall should break into chunks and then stuff that is like smaller than a set size like 0.1m or 0.01m then it just is visual and disappears probably , weak/strong device thats good separation  by that devices capabiltieis 
7. graph is vr good, node graph, yes do center of mass vs other contac tpoints
8. that is good i want that to be how it is 
9. that is good too 
10. thats good 
questions 
1. 

# physical moving objects

1. evethingin the engine is a phsical object ideally. all things should operate on the same fundamental rules, and so can interact iwht each other using the same fundamental simple fuctions, like  sphere vs box or  box vs box etc, no specialized funcitons . so everthing is a phsical object 

2. server in v1 shouldl be final authoirty, so all plauers can agree on one position for real, but over time i do want it to be more client sided. but for now, clients send input requests and stuff to the server, and what they hit and where the thing they hit shoudl be, and the thing should be tracked with entity id system or ecs idk 

3. yes, that is what i want to do, i dont wnt to  just send the position oevr and over. unless, tats not a big deal? im thinking of scale, if we haev 100,000 phsics objects all getting moved around at once, wahts most efficient, i think its prob not sending each ones individual new pos transform probably that determinsitic client/server function that both of them share 

4. client gets that object smoothly moved to where it actuallt should be instead of where client predicts it is, over time this should be  like, waht client A sees = what clients B, C, D, E, F, G... see = what servr sees, so no disagreement can even happen at all 

5. yes that is 1000% accurate everthing should be mass and forces, i will just tweak the numbers to match the feel i want 

6. walking into a crate should push it based on mass and stuff. standing on it can trasnfer weihgt, but more likely  landing hard on it should transfer velocity and momentum, u might break it if u hit it too hard. same with players, players should be stand on-able, and take damage/knockback from other players hitting them with bullets or projectiles or melee etc 

7. absolutely yes that is how we should do it , ragdoll and all things else should emerge from that simple ssytem, and we should expand that simple ssytem over time to handle more cases but still it should be simple and generaliezd to handle anything  we want to do with it, wherther its ragdoll, arm attached to torso, plr grabs worlrd, plr grabs another plr, sticky grenade attached to a surface etc etc all supported with 1 function 

8. yes, it shouldl be  like, the same as netwokring has just 1 attackreqeustpacket generic, we should prob expand that or supersede it with like a collision/interaction request, so plr vs box, box vs box, grenade vs grenade, rocket launcher weapon model vs world, etc, all handled same way, super efficieint. and this should make it naturally emerge the result of these interacitons, no hardcoding speciifc functions to do something 

9. yes it should work on antthing that moevs and breaks etc. crate has that bullet hole applied to it, and also the hole is in that crate as it moves and gets thrown around. so shoot the front of it, rotate it 90 deg = the bullet hole is now 90 deg away not just same surface. its also  a actual volume taken out of the object, not just a decal, although not sure how to balance blood and bulelt holes and debris and world imapct effects with performance

10. max acitve objects idk, prob define in like  C:\mimita-priv-v8\config\destructible-world.json here, defien all that stuff here . but yes that gameplay movement and fun gets  that priority. and yes i wantthousnads, milliosn , billiosn, an indefininte amount of sleeping objects possible per frame, just need more efficient code over time and its pissible 

explain the summary of the current plan ur thinking  and then aks more questions to get it more detailed further and better 

# blood and viscera and gore etc

1.  yes, we currently hav a effect system and it works for like dash effects and stuff but we shoudl ultra expand that to handle antthing , like, wall craacks, world debris, etc, blood, bullet holes, gore viscera etc

2. thats good but idk about this  cuz the blood and stuff it was good before , i have a video of it, but it ran super bad, so not sure how to do this im confused here 

3. it should b epersistent but scale with LOD and client  requirements, same as world destruction like should be persistent but it should not cost more performance, it should over time be as efficient as we possibl can get, to get the same end goal of, shoot a crate 5 times, play 12 mroe hours in that same server, come back to that crate , it still ahs 5 bullet holes, as well as antthing else that i interacted with. todo, storing all this data in 1 server that lasts a long time, not sure how, matbe distributed across multiple users liek hashes or something? or just like a specific seed that has all that? imnot sure 

4. yes unl,imitetd, with batching and optimizationover time, dont render imperceptilve info

5. do not simulate it if we cant even see it , dont even render if its behind us  dont even spend energy on it, when welook at it again then render it again 

6.  a bullet hole should be a  actual hole, C:\mimita-priv-v8\docs\specs\destructible-world\destructible-world.md  relatingto this document . crater is actual changed geometry, smoke can obscure vsiiblity, etc

7. yes to all of that and yes it should all be json editable

8. cam shake, plr pushed from blast, ears ringing, cam sway, muffled audio , it should be that function like strength X distance not just  on/off, following the force function we alread have of  like, angle how dirct the angle and how high the speed/velocity 

9. yes measruareble rules, the overall tihng is as lcose to 0ms frame times as i can get, yes never gameplay slowdown, effects may at most use  this much cpu and gpu i agree, and measure that in ms or idk how to make it  something taht all operaitng systems and devices agree with but yes 

10. we prove it  over time whewn it becomes reliable, we cant reallt do testing yet i think  but we should do auto testing after we have proven it manual like human testing i predict if we doit like that then since i saw what good behavior is like  i can hlpe design an actual test rather than  letting AI guess it, falsifiable is if we do tests now and design around tests proving itself first but i rather do  the hpothetsis first 

explain the summary of the current plan ur thinking  and then aks more questions to get it more detailed further and better 

# ragdoll mode 

1. plrorigin is part of the body sitll but now u are a phsics object basically. its plrorigin auth root,plr root pos, then torso is attached 1:1 with that i think, then arms and legs and head are attached to the torso with an attachemnt, and cant overlap with themselves and u cant fall thru the world, capsule around each limb and torso and head . yes ur bod parts all become phsics driven, and u  do it with G  , so press G once = ragdoll mode enter, G again = ragdoll mode exit . exit should do like a hop up so u dont exit into the ground 

2. thats true, those kes are repurposed whne ragdolled, u cant mmove left/right with A/D , u are a ragdoll so u have to like use ur arms to grab the world and move around etc 

3. theh exact first world point, but prob raycast, i like the little grace period distance like of 0.5 meters , u just will grab it if its close enough, but  taht should be hot reloadable.  C:\mimita-priv-v8\config\ragdoll.json here hotreload here 

4. stretch slightly, the thing that connects is ok. hand is here, wall is there. press lef tclick, extend m left arm toward the wall, and as i hold lmb down, left arm is extended, indefinitely no stamina. i press A and hold A = if im close enough, grabs the wall. now, i let go of LMB , i still hold A = im holidng onto the wall. arm still is free and gravit affected, whole bdoy is grav effected. but we need like a gravity thing like so we dont just keep falling over and over and gain huge huge amounts of downward speed. anad then in this , holding A means the left arm is on the wall. now, i can press/hold RMB, extend the right arm, and my head bc the camera is locked do wherever the head is looking in this  mode, and head moves smoothly bc of the new  smoothm ovemnt in C:\mimita-priv-v8\config\aimbody.json should be implemented,  smooth camera movement with natural  swaying from the bod parts moving, i can extend right armwith RMB, and i can equip a weapon and shoot . so RMB os held down, press 1 rto equip the revolver, press LMB to shoot = it will shoot the weapon and extend my left arm, i think we should  make a hot reloadable mode so that it either, arme xtends while weapon equipped = true or false, keep it true for now 

5. hold infinitely, prob later we do more realism but that is not fun, it should be hold infininte with infinite stamina , more arcade style grab 

6. lmb down = extend left arm, like. as soon as u press it donw, it tries to extend far out as it can but is limited by the shoulder attachment, to wehrver ur mouse is pointing, cant go thru walls tho, but no stamina. so lmb down = holding ur arm out in front of u in real life. same with rmb down. letting go = it falls limp to ur side and gravity inluences it now 

7. arm has a target extension position and its prob just camforward, wherever ur camera is pointing at that time is where it wants to extend to , not like crosshair mode where it hits the thing ur aimed at, no , its like almost a physical mode, casting forward from ur camera forward infinitely = along that line is what the arm extends toward. im confued about heavier or lighter things  but i do like that idea 

8. entirely produced by collisions and grabs, legs just dangle, as well as torso. its just ur arms and head that matters in ragdoll mode the legs and torso are just gravity limp 

9. yes, the damage hurtbox is the actual part that u hit, the movement/collision hitbox is larger and more forgibing as a capsule arund each bod part, head torso left/right arm left/right leg 

10. preserve velocity, and instantly become upright . yes head/torso/arms/legs should have their own masses as well

and then we prob should expand the  moveing objects physical thing first  too 

explain the summary of the current plan ur thinking  and then aks more questions to get it more detailed further and better 