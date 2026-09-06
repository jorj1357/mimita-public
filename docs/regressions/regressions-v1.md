// 09 03 2026, 15 42
/* purpose
* record confirmed behavior breaks discovered through human review or playtesting
* preserve the exact wrong behavior, cause, fix, and proof for future agents
* connect each regression to the changelog session that introduced or fixed it
* this file DOES NOT record normal AI work history
* this file DOES NOT replace current specifications
* this file DOES NOT allow old entries to be rewritten or deleted
*/

# MiMITA Regression Records

This is an append-only record of confirmed regressions. Normal AI work history
belongs in `docs/changelog/`. A new entry must include the expected behavior,
actual behavior, exact specification, wrong code, corrected code, cause, fix,
proof, observed time, and related changelog file.

Whats this

- 9 2 2026 this tracks like  
- Ok if the behavior we want worked before, then later, it stopped working,   
- Write it here and why and then what fix etc 

9 2 2026 format

1. Issue: lala  
   1. Bad behavior  
      1.    
   2. Date and time first observed:   
      1.    
   3. Why bad behavior  
      1.     
   4. What fixed it, date and time  
      1.  
   5. What we learned  
      1. 

newest at top 9 3 2026

9 3 2026

1. Issue: the website https://mimita.fun the signing up and logging in is broken 9 3 2026 1526
   1. Bad behavior  
      1.    i go to the site and i log in and it sas database connection failed. check server logs.
      2. thats not good and sucks dick
   2. Date and time first observed:   
      1.    this hapepned before, but its happened first viewed at  9 3 2026 at 201 am est  bc of a discrod message in mimita discord
      2. viewed again  9 3 2026 1527
   3. Why bad behavior  
      1.    i need to be able to log in like thats a  core, if mimita is a tree, logging in/signingup is the roots of the tree, thats so so so imporatnt to work 
   4. What we learned  
      1.    
   5. What fixed it, date and time  
      1.  

9 2 2026

1. Issue: ui making performance suck dick   
   1. Bad behavior  
      1.  It suckds dick and t  
      2. Takes like 10ms on a integrapted graphics computer  w no gpu  to render ui thats bad   
   2. Date and time first observed:   
      1.  Like  prob aug 1 2026 but recent was  aug 30 2026  
   3. Why bad behavior  
      1.  Bc all the ui was getting drawn   
   4. What we learned  
      1.  Do not draw all the ui all in one, matter of fact  
      2. Batch evreuthing we can  
      3. Dont collide with all triangels in the whole world either  
   5. What fixed it, date and time  
      1.  Batching ui calls into 1 singel call  
      2. Made fps go to liek 100 minimum on a family computer  
      3. Just needs better  tweaks long term for better perofmance etc   
   
2. Issue: collisions sucked dick and made huge frame time lag when getting close to a big clinder  
   1. 9 2 2026 fill in later bc thats liek mimita preview liek v2.14 on mimtia youtube channel

9 6 2026 resolution for the 9 3 2026 website authentication outage

1. Expected behavior: `https://mimita.fun` signup and signin requests can query
   the account database and return normal validation or authentication results.
2. Actual behavior: PostgreSQL cluster `14/main` was down, with no PostgreSQL
   process or listener on port 5432. The online `mimita-api` process returned
   `500` and `ECONNREFUSED` for `/api/auth/signin`, `/api/auth/me`, and
   `/api/site/banner`.
3. Why it happened: the API's configured database dependency was unavailable;
   the application could not connect to PostgreSQL. This was an infrastructure
   service outage, not a bad username, password, signup form, or frontend route.
4. What fixed it, 9 6 2026 08:33 EST: started only the existing PostgreSQL
   `14/main` cluster on the VPS. It became `online` and `pg_isready` reported
   `accepting connections`; no code or VPS files were changed.
5. Proof: a correctly formatted non-mutating invalid-signin probe returned
   HTTP `401` with `invalid username/email or password`, proving the auth query
   path reached the database instead of failing with HTTP `500`.
6. Related session record: `docs/changelog/09-06-2026/09-06-2026-08-34-02-website-auth-recovery.md`.
7. 9 6 2026 0900 extra note: SO ENSURE THE VPS IS ALWAYS RUNNNING, AND IF DATABASE ISSUES HAPPEN, NEED TO BE ABLE TO START THE VPS FROM MOBILE PHONE NOT JUST PC

9 6 2026
1. Issue: all UI was drawn individually calls, making perfomrance very bad on low power devices and all devices in general. goes directly against a big assertion/invariant of  as close to 0ms frame times as we can possibly get, for any device at all
   1. Bad behavior  
      1.    all UI renderd individually, meaning each new letter = new draw call, unecessary work
   2. Date and time first observed:   
      1.    not sure it has been like that for like months maybe like 5 1 2026, but observerd again in a bad way like 8 31 2026s
   3. Why bad behavior  
      1.     because we didnt even know the code was bad bc i just assumed that it was fine, and also bc the pc i use to code it is powerful device so its not fair
   4. What fixed it, date and time  
      1.  we fixed it sometime between  8 31 2026 and like 9 6 2026, its in the git commit history, i committed from the low power device ihave
   5. What we learned  
      1. sometimes code is not efficient even if it seems like it, like who would thiink teh UI is taking 3ms to render every single frame, so question all assumptions over and over bc it might be the most randomest little thing making the issues happen 

   ## 2. 9 6 2026 2000 — Articles lost after VPS git pull

   1. Bad behavior
      1. 20-30 articles created through the admin editor at /admin/articles disappeared from the live site
      2. Only 1 article remains (welcome-to-mimita-news.md)
      3. The /articles page shows the article but clicking it does nothing (broken link)
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. Articles are stored as .md files in content/articles/ on the VPS filesystem
      2. These files were never committed to git — they existed only on the VPS
      3. When we ran `git pull --ff-only` to deploy the data-saving persistence, the content/articles/ directory was updated to match the repo state (which only had 1 article)
      4. The untracked .md files were overwritten/lost by the pull
      5. Additionally, ArticlesIndex.jsx used `article.url` but the generated JSON has no `url` field — only `slug` — so links were broken even for the remaining article
   4. What fixed it, date and time
      1. Fixed ArticlesIndex.jsx: changed `article.url` to `/articles/${article.slug}` (9 6 2026)
      2. Articles cannot be recovered from git — they were never committed
      3. Added rule to vps-deployment.md: content/articles/ must be committed to git so articles survive deployments
   5. What we learned
      1. Files created on the VPS through the admin editor are NOT automatically committed to git
      2. `git pull --ff-only` does not preserve untracked files in tracked directories
      3. Always commit user-generated content (articles) to git before deploying
      4. The ARTICLES_DIR path in admin.js resolves to content/articles/ at the repo root — this directory must be tracked

   ## 3. 9 6 2026 2000 — Profile stats not visible on live site

   1. Bad behavior
      1. The profile page at /users/admin did not show gold, XP, kills, deaths, or playtime
      2. The API returned correct data but the frontend didn't render it
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. ProfileStats.jsx and persistentStats.js were added to the repo on 9 6 2026
      2. The VPS frontend dist was built on 9 1 2026 — before these components existed
      3. The deployed JS bundle had 0 matches for ProfileStats, playtimeTicks, or formatPersistentStat
      4. The VPS deployment procedure did not include a frontend rebuild step
   4. What fixed it, date and time
      1. Added "rebuild frontend" step to vps-deployment.md (9 6 2026)
      2. Ran `npm run build` on VPS to rebuild dist with new components (9 6 2026)
   5. What we learned
      1. Every deployment that touches website/src/ MUST rebuild the frontend
      2. A stale dist/ serves old JavaScript that may lack new components
      3. The deployment procedure must include `cd /root/mimita-site/website && npm run build` as a mandatory step