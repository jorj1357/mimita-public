2026 11 05 1649 

jorj tod explain its

reuse teh deathball impleentation stuff but like 

the network gets like

tick 1 = input detecetd, like 0.5 meter capsule of fire damage volume 

this is like input down/input up for networking, clients render fire themselves

it has some lag as well,its fire, cant just aim at someone isntnatly and the fire is aiming right at them but w should have it deifned lag in like the weapons definition expand eweapon definition to be able to define that

and todo how to amek it so we dont have 1 single weapon json that has all the fields, its just it has the required fields for the wepaon? idk 

but input down tick 1 = little capsule, like 1.0 meter long and like 0.5 meter diameter 
long ways acpsule ends are at the ends of it 

tick 30 of holding = max size of it, like 4.0 meter long like 1.5 meter diameter 
and its that big indefinitely for as long as its hled and u ahve ammo

ammo = send i think in chunks? like batching? 

it should use ammo like use 1 ammo per tick

so 60 ammo consumed per tick

u should be able to fire for 30 secodns straight

so like 60 * 30 = 1800 ammo?

reload like 2 sec
reserve ammo like 3600 for now, but can be made higehr lower, ammo consumption rate higher or lower etc

when no ammo , dry fire does nothing , curernt dry fire path use that 

clients get the input began/ended packets
but
presented to them visually as
spawn or put a png facing effect maybe for each tick that the fire is held
and it should be liek a fire png 

and it should like progress thru phases or 
i prefer it to be procerduarl generated png actually like randomized decal 
but

start = like white hot, middle = like orange yellow, end like dark red

white = like 1.0 alpha full opaque
orange yellow = like 0.7 alpha
red dark = 0.5 alpha

at white/close range = 

like 5 damage per tick u are intersecting it , per how close u are to the centerof the spehre , 5 dmg = within 0.5 meters of th exact center , the limb/part of u intersecting the volume, the closest point on that part is how close u are to the center of it, 

so if edge of head in the edge o fthe volume, low damage like 1 damage per intersecton, 5 dmg when hte point is within 0.5 meters of the exact center of the fire

yellow/orange mid range = like 2 meters = like 3 dmg at exact center, 1 dmg at edge

red/dark end range = like 4 meterrs, liek 2 dmg at exact center 1 dmg at edge

minimal knockback like 0.01 knockback per damage 

lasting damage like
for 300 ticks after u exit the fire, u take like 1 damage, N damage per N ticks after u leav  it, until 300 ticks is up

e.g.

tick 1 thru 30 = intersecting the fire volume, lots of damage
tick 31 = stopped intersecting

tick 31 thru 331 if tick limit is 300 = every N ticks, e.g. every 10 ticks, take N damage, e.g. 1 damage

tick 332 = stop taking damge, not on fire anymore

need a on fire flag presentation, expand client blood presetnation to show fire on self, adn events sent to others so the client recieves like

onfire= true for entity ID 12345
means, clients recieving that event render that entity on fire 

uses weapon new weapon model, needs 

equip sound, fire sound like each ball makes a sound
mode 1 = each ball u spawn maks a sound
mode 2 = constant sound playing when firing is held down 

orange/mid range 