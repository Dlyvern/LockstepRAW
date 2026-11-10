# Mission 1 - "Saint Abel" (title OPEN)

Level 1 = morning -> station -> hospital -> chair -> cut to interrogation.
Players stay together the whole level (no separation). NPCs may say "you two" here only.

## Part 1: Morning (implemented)

The two leads wake up in their flat, get their gear from their lockers, walk through the park
and drive to the station. (Game/Level/MorningRoutine.)

## Part 2: The station (~5 minutes, no horror)

Dialogue: `Data/dialogue/station.txt`.

1. **Lobby** - desk sergeant greets them, tells them the chief is upstairs in a mood.
   Objective: `station.objective.chief`.
2. **Chief's office (2nd floor)** - briefing (`station.chief.briefing.01-14`): jabs about being
   late and broke; then the call: armed men have taken Saint Abel Memorial, staff are dead,
   hostages are held in the basement. The city team is tied up across the river, so the 14th's
   response team goes. The chief's tells (deniable): he already knows where the hostages are,
   overtime is suddenly approved, he tells them to keep going down whatever they see, and he
   knows they look after each other. Orders: armory, then the car; the range is open. The
   briefing starts only when both players are at the chief's desk.
3. **Bullpen** (on the way down) - chatter (`station.npc.idle.*`): the hospital on the news,
   dispatch full of static.
4. **Basement armory** - armorer hands out armor, the V-45 SMG and ammo.
   Objective: `station.objective.armory`.
5. **Holding cells** (same basement) - guard and prisoners; optional, flavour and two hints.
6. **Firing range** - optional; never blocks progress. Tip: `station.tip.range`.
7. **Out to the car** - objective `station.objective.car`; drive to Saint Abel Memorial.

## Part 3: The hospital

Dialogue draft: `Data/dialogue/hospital.txt`. Level does not exist yet; node and room names
below are proposals for the level-designer.

Bullet-time: **none** until beat 5. No powers of any kind before the chair.

1. **Arrival and upper floors.** Cordon, smoke from a ward window, the ambulance bay. The
   squad splits by entrance on the radio; the leads take the ambulance bay into the ER. Normal
   SWAT action: lobby, ER, 2-3 ward floors. Civilians hiding, dead staff. The gunmen fight like
   trained mercs (terse callouts, cover each other, fall back in order) - planted, never said.
   The chief on the radio: "hostages are in the basement, go down."
2. **Clues on the way down.** The gunmen aren't holding floors; they move things UP from
   below: sealed crates on the stairs, a keyed service lift running up, the security office
   with every feed live except the basement's (cut). The squad notices; the chief waves it off.
3. **Descent.** Service corridors, laundry, the morgue. Then a door marked as the sealed East
   Annex ("old research wing, closed"), not on the plans the city gave them. Tone flips: clean,
   cold, no patients, no blood. Fewer, harder guards who barely speak.
4. **The lab.** Discovery: an empty, clean lab; two chairs with restraints, side by side; no
   hostages. Then gas. Both players pass out. Hard cut.
   **OPEN:** did the chief trigger the gas remotely, or the firefight?
5. **The chair.** Same room. Both wake restrained side by side. Lights flicker. Power cut,
   the restraints come off. "Monsters" break in. Bullet-time triggers automatically (unlimited)
   for the first time; the players shred them; it should feel incredible. Real blackout.
   Title card. The escape is not played.
   See bible SPOILERS for what the monsters are; rules for building them are in
   `foreshadowing.md` (second-playthrough details).

### Route and decisions (user, 2026-11-10)

Supersedes the beat details above where they differ. The level is about 40-50 minutes, split in
two cooked levels at the service lift ride (the ride hides the load).

**Part A** (`levels/hospital_a`): outside (cordon; the rest of the squad stays outside) ->
ground floor (lobby, ER) -> two ward floors with gunmen -> survivors (staff barricaded in a
room; the players leave them there, the chief's radio pushes on: "hostages are below") ->
service lift down.
**Part B** (`levels/hospital_b`): underground parking (first deniable horror: power faults,
static, something gone when the flashlight gets there; no monsters on screen) -> the East
Annex ground level (gunmen) -> basement (services, morgue) -> the old research wing -> the lab
(gas, then the chair).

- The leads go in with **two AI squad officers**. They die easily. They are **separated in the
  underground parking** and come back among the "monsters" in the chair room (their bodies show
  it on a second playthrough). The rest of the squad comes down later; they are the other
  "monsters".
- Gore: brutal. Bodies with blood.
- Gas trigger: still OPEN.
- Hospital: Saint Abel Memorial Hospital; the second wing is the East Annex.

## Next scene (not in this level): the interrogation

A bare room. The chief, alone with them (or with one of them - see bible: after level 1 NPCs
address one person only; **OPEN** how this first scene stages that). He tells them the squad
is dead and it's their fault. No dialogue drafted yet.

## Open questions

- Mission title.
- Gas: chief remote trigger or firefight?
- Does the chief stand at the cordon in person (seen once) or only on the radio?
- Is Rodriguez on the squad?
- Title card text (`hospital.caption.title` uses "LOCKSTEP" as a placeholder).
