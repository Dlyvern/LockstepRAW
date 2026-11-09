# Mission 1 - "Welfare Check"

## Part 1: Morning (implemented)

The two leads wake up in their flat, get their gear from their lockers, walk through the park
and drive to the station. (Game/Level/MorningRoutine.)

## Part 2: The station (~5 minutes, no horror)

Dialogue: `Data/dialogue/station.txt`.

1. **Lobby** - desk sergeant greets them, tells them the chief is upstairs in a mood.
   Objective: `station.objective.chief`.
2. **Chief's office (2nd floor)** - briefing (`station.chief.briefing.01-12`): jabs about being
   late and broke; the case is a welfare check on Ida Pruitt, 71, apartment 4C of the Garland
   Arms on Carver Street. Her daughter hasn't reached her in three days. The building is where
   the Carver Street boys keep product, and the last two calls there met nobody. Orders:
   armory, then the car; the range is open if they want it. The briefing starts only when
   both players are at the chief's desk.
3. **Bullpen** (on the way down) - chatter (`station.npc.idle.*`), including a warning about
   the Garland's lift and a note that dispatch is full of static.
4. **Basement armory** - armorer hands out armor, the V-45 SMG (fictional designation, see
   bible "Weapons") and ammo.
   Objective: `station.objective.armory`.
5. **Holding cells** (same basement) - guard and prisoners; optional, flavour and one hint.
6. **Firing range** - optional; never blocks progress. Range officer intro, players can shoot
   paper for fun, done lines. Tip: `station.tip.range`.
7. **Out to the car** - objective `station.objective.car`; drive to the Garland Arms.

## Part 3: The Garland Arms (PROPOSAL - OPEN for the user)

- The super refuses to help, then can't be found. The building is too quiet for midday.
- Floors 1-3: ordinary tenants who won't open doors; signs the crew left in a hurry.
- Floor 4: the Carver Street boys are still there, armed and jumpy, barricaded. They opened
  fire on the last people who knocked. First real firefight; bullet-time introduced here.
- Apartment 4C: Ida Pruitt is alive, hiding in her bathtub. The crew had been holding the
  floor against "something" they won't name; to the players it reads as paranoia and drugs.
- Ending: they bring Ida out. Back at the car the radio is all static for a few seconds.

Why it works: grounded and plausible for two patrol cops, escalates to combat naturally, and
leaves a thread (what scared the crew?) that the horror can pick up later without having
been shown.

**OPEN:** keep Ida alive (warm ending, someone to care about) or not? Does the crew's fear
point at the same thing that later hits the station?
