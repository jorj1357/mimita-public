9 6 2026

todo this not good but move tothis version more  more general things  for gamemodes
i want to literally be able to support 100 trillion different gamemodes at the very aboslute least super ultra scalable bc other people i  act as if they aregonna make their own gamemdoes so we need ways to diffrentiate between two modes 

can we make it so it doesnt need else if? it doestn care if thre is 5 modes or 500,000 , it just  works for any mode u give it  according to that mode's json setings and stuff

like we dotn  do it with  f, or t, or b
no else if  match mode  being a sepcific thing that seems like it would break super ez, we should do like 

the json alreadt defines an id bombtag  
like this C:\mimita-priv-v8\config\gamemodes\bombtag.json 

so just check wahtever that  id is and look for the data from  that json and use that as the intermisison and countdown and game start etc 

so we shoudl achnge the  underling logic underneath , the code should not do hardcoded things like if the name of the mode starts with this specific letter like that is  very fragile and if files move or things are renamed it will break, it should work  based on the id in the json for now, and not require  exact letters matching 

also, somehow  to change from bombtagmanager, this is too specific, m ake it like
gamemodemanager

BombTagManager::renderHud() — renders bomb holder text and timer from replicated state
BombTagManager::renderBombVisual() — renders bomb sphere and world timer - like all this, just change it to be a general function that ahndles all the hud and stuff, that the json requests u to render. so this should work for not onl a  bomb with a world timer, but maybe, a boss with a healthbar above their head, a car with a healthbar u need to destroy, a npc that is telling u something with world text visually, etc. super general 

9 2 2026

- End goal  
  - Clearly explain what each gamemode we are doing   
    - Bc we are moving away from duels queue and just doing community server browser from now on  
  - How each gamemode behaves, how it is to use the gamemodes etc  
- Meta  
  - Generalization of functions  
    - Gui elements should be made with general functions  
    - These gamemodes are just different definitions of the same json structure, no functions should be written directly for hjust 1 mode, all functions should be general so later we can expand the modes 

Commands, for host only, 9 2 2026 0922 move this into its own commands spec 

- “Modelist”  
  - Lists all modes that are json defined   
  - As of right now, the path is here  
    - "C:\\mimita-priv-v8\\config\\gamemodes\\ffa.json"  
    - "C:\\mimita-priv-v8\\config\\gamemodes\\tdm.json"  
  - And the team death match json for example looks like thsi   
    - {  
    -   "id": "tdm",  
    -   "name": "Team Deathmatch",  
    -   "description": "Red vs Blue. First team to 30 kills wins.",  
    -   "team\_names": \["RED", "BLUE"\],  
    -   "goal\_value": 30,  
    -   "time\_limit\_seconds": 300,  
    -   "respawn\_seconds": 0.01,  
    -   "kill\_heals": true,  
    -   "countdown\_seconds": 3,  
    -   "go\_seconds": 0.75,  
    -   "rematch\_seconds": 5,  
    -   "spawn\_offset\_radius": 5.0,  
    -   "spawn\_strategy": "shared",  
    -   "intermission\_seconds": 15,  
    -   "results\_seconds": 8,  
    -   "maps": \["mimita-duels-map-v3", "atdm", "funworld"\]  
    - }  
- “modestart”   
  - “Modelist” lists tdm as 1, ffa as 2  
  - “Modestart 1”  starts tdm, at the intermissionstage  
- “modestartnow”   
  - “Modestartnow 1” starts tdm, at the 3,2,1, countdown stage  
- “Changemap”  
  - “Changemap 1” changes the server’s map to whatever shows up as the first map   
  - “Changemap 25” changes the server to whatever map is the 25th map in the map list   
- “maplist”   
  - Lists all maps, with a number, so u arent typing “Superlongmapname12345” u just type “changemap 17” and it loads that map  
- “Weaponsetlist”  
  - List all weaponsets  
- “weaponsetpick”   
  - Pick a new weapon set for all plauers , and apply it instantly   
    - And npcs?  
- Npc behavior console commands  
  - Todo, difficulty,   
    - maybe npc names defined in a json,   
    - And each npc can have behavior like  team deathamtch, what team theyre on, etc  
  - Also is npcs good enough to hav elike 15 in a server? And it runs fine? For liek team death match and stuff   
- Npclist  
  - Lists all npcs,   
    - 1: npc-bob  
    - 2: npc-ben  
    - 3: npc-carson  
    - 4: npc-david  
- Npcteam (n) (team, red/blue)  
  - Npcteam 3 red  
    - Means npc-carson is now on team red  
  - Npcteam 1 blue  
    - Means npc-bob is now on team blue  
  - They stay on these teams until they are deleted  
- Npcdelete 1  
  - Means npc-bob is deleted  
- Npcdelete 1,2,3  
  - Means npc-bob, npc-ben, npc-carson is deleted 

Commands, for host or player

- Changeteam  
  - For now behavior, u can join whatever team u want to join, ever. No team limits, etc.  
  - Ideal behavior: the teams are equally sorted by skill.  
    - If u get like 2 kills per whole 5 min round, u are gonna go on the team with  better plauers to balance it out  
    - If u get like 25 kills anad u win games by urself, u are gonna get a team with not so good players  
    - I know, it sucks, and def needs a better solution, bc it should not be their fault that u lose, it should be your fault, but this is a casual ish mode for now, so its fine. Later we have better team sorting, or no skill taken into consideration at all, idk.   
  - Teams for now: red, blue  
  - If you just write “changeteam” \= prints all teams that u can switch to, and how many people are on each team, and their usernames  
  - “changeteam red” \= you requests to join the red team

- 

Weapon set

- Todo explain this 9 2 2026 0848   
- And also the hotbar behavior 

Full ideal flow of behavior

- Start mimita.exe, im on teh main menu screen  
- I press play   
  - I instant go to the communitt server browser, and also community server creator. Bc thats all in 1 screen  
  - I do not go to duels queue, that button is not even in the gui at all.  
- I make a server with these settings  
  - Name: my super cool server  
  - Gamemode: team deathmatch  
  - Weapon set: rocket launcher only   
  - Map rotation: enabled  
    - Auto picks a random map to rotate to  
  - Notify discord that the server started: enable  
    - Todo fix this so online servers are shown to be online, not just left there for no reason 9 2 2026 0853  
  - Map: xxity2 because its a new cool map  
  - Privacy: public  
    - Private \= just a password needed to join  
    - Todo make more cool later but for now thats cool  
  - Npcs spawned: true, just 1 spawned  
- I press start server  
- Im instantly in the server, and im getting shot at by the npc  
- Im fighting the npc using the rocket launcher, bc the npc onlt uses the rocket launcher and i can only use the rocket launcher  
  - Which is the only weapon in my hotbar, becasue the weapon set is rocket launcher, so thats the only one i can even equip.  
  - And its on slot 1 , not slot 7 like it was before   
- As soon as i join thte server, because i picked team deathamtch as the mode for the server, i see  
  - “Mode: team deathmatch, intermission: 15”  
  - “Mode: team deathmatch, intermission : 14”  
  - Etc  
  - Counting down , in big text in the center of my screen, not in the chat, because the chat is not big enough  
- As soon as the 3,2,1, countdown before the gamemode starts, teams are picked.  
  - For example, previous player list, all just on the same team/no team at all  
    - Jorj1357  
    - Admin  
    - Jojo  
    - Rickets  
    - Andrew  
    - Npc-derek  
    - Npc-jimbob  
    - Npc-smoothmovement  
  - After team picking  
    - RED  
      - Jorj1357  
      - Jojo  
      - Npc-derek  
      - npc-jimbob  
    - BLUE  
      - Admin  
      - Rickets  
      - Andrew  
      - npc-smoothmovement  
- intermission timer  
  - It counts down from 3, to 0 , with the 2nd decimal place.  
  - So its like 3.00, 2.99, 2.75, 2.17, 1.53, 0.85, 0.21  
  - As soon as it gets to 0 it is replaced with “GO\!\!\!”  
- Then, team death match starts.   
  - A timer at the top , that does not obscure other gui elements at the top of the screen, has the match time, for example 5 min, and counts down   
    - Hh:mm:ss.miliseconds  
  - So like  
    - 00:04:59.982  
    - 00:02:12.255  
    - 00:00:15.174  
    - Etc etc   
  - Also, it starts \= every single person regardless of team, is spawned at 1\. The same spawn point, with a random offset alongn x/y , because x/y is thehorizontal plane in this engine, z is up in this engine   
  - For the rest of the match, everyone spawns at taht same spawn point, with a random offset, and instant respawns  
    - Every single time  
    - I know this is unbalanced  
    - We will add forcefields or some sort of thing to push you out like  
    - Spawn pos \= x  
    - Your random offset \= \+1x \+2y  
    - So , the exact line between spawn pos and your random offset \= your push out direction. So you get shot out from it and are hard to predict where ur gonna go , ties into movement of the game  
- Gui   
  - At the top of the screen   
    - RED on left  
    - BLUE on right  
    - “RED: 12K                        BLUE: 10K”  
  - When u get a kill   
    - You personallt get the popup reward of like   
      - If plauer kill  
        - \+100 xp in turqoise  
        - \+50 gold in gold color  
        - This is tracked to ur account   
      - If npc kill  
        - \+10 xp in turquoise  
        - \+0 gold in gold  
        - This is also tracked to ur account  
      - If no account  
        - No popup, bc nothing to track?  
        - Or do a popup of \+0 gold \+0xp, because its satisfying and gets u used to it  
    - Your team gets a green “+1”   
      - It starts at the kills part of the gui, like   
      - If ur on blue  
      - As soon as the kill is confirmed  
      - Show a little green “+1” under the team’s total kills , it shows at like 0.5 alpha 50% opaque, then fades to 0.0 alpha 0% opaque over 30 ticks. This is json editable, and should be controlled by a generalized json function   
  - When you press tab  
    - RED  
      - All players on red  
      - All players kills, deaths, assists  
      - Top \= most kills  
    - BLUE  
      - All players on blue  
      - All players kills, deaths, assists  
      - Top \= most kills   
- Win conditions  
  - Either, kill enouhg epople on the other team to get to 30 (?) i think thats the score u need  
  - Or, have more score than the other team by the time the timer is done  
- When a round is won  
  - Show “RED TEAM WINS\!\!\!\!”  
  - Or “BLUE TEAM WINS\!\!\!\!”  
    - Show how many times that team has won in this server so far  
    - E.g. a new ish server will have like, red team’s total wins: 3  
    - Blue team’s total wins: 2  
    - For now, 9 2 2026 0916 bc i cant think of the right gui places to put that, just put that in the chat every time that the game is over  
  - Show all players on that team, like put their scores in the gui as well, so itll be like  
  - At the top: “RED TEAM WINS\!\!\!\!”  
    - Jorj1357: 13K, 5A, 8D  
    - Admin: 10K, 1A, 2D  
    - Etc, for all players, and npcs too   
  - This screen is shown for 5 seconds, with a timer counting down how long its shown,   
  - Then we immediately repeat from the intermission step above, where it starts at 15 seconds.   
- This repeats indefinitely  
  - The entire flow should repeat indefinitely. I should be able to 