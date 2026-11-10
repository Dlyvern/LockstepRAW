# Foreshadowing

Each hint: where it appears, what it looks like, what it pays off. Keep them rare and
deniable - each must have an ordinary explanation. Payoffs refer to bible SPOILERS.

## Station (written)

| Id | Where | Hint | Ordinary reading | Pays off (OPEN where marked) |
|----|-------|------|------------------|---------------------------|
| `station.prisoner.09` (with `station.guard.idle.03`) | Holding cells | A prisoner asks the time, then says "it was that an hour ago too". | A drunk who lost track of time. | Time goes wrong around the leads; ties to bullet-time, the shared power. |
| `station.range.idle.01` | Firing range | Lane four "pulling left all week", nothing wrong with it. | A worn lane, a fussy range officer. | OPEN: were the leads already being dosed before the raid (their perception is what's off)? |
| `station.prisoner.08` | Holding cells | His cousin, a porter at Saint Abel, says nobody's allowed in the basement. | Hospitals restrict basements. | The hidden East Annex under the hospital. |
| `station.chief.briefing.06` | Chief's office | He knows the hostages are in the basement before anyone's gone in. "Don't ask how we know." | Intel from a phone call or a hostage. | The chief is in on it; the hostages are bait. |
| `station.chief.briefing.10` | Chief's office | "Overtime's approved." In a precinct where overtime is frozen. | A real emergency loosens the budget. | Someone outside the city is paying for this. |
| `station.chief.briefing.12` | Chief's office | "Whatever you see down there, you keep going down." | A tactical order. | He's herding them to the lab. |
| `station.chief.briefing.13` | Chief's office | "You two look after each other. You're good at that." | A rare kind word. | They were chosen because they are close. |
| `station.npc.idle.06` | Bullpen | Dispatch full of static all morning. | Radio trouble in a broke precinct. | Radio is the chief's channel; static recurs at the lab when the gas hits (`hospital.chief.radio.lab.03`). |

Not hints (grounded setup only, keep it that way):
- `station.npc.idle.04` - the hospital on the news.
- `station.npc.idle.05` - Rodriguez's cake Friday. Only lands later if she's on the squad (OPEN).

## Hospital (drafted in `Data/dialogue/hospital.txt`)

| Id / place | Beat | Hint | Ordinary reading | Pays off |
|------------|------|------|------------------|----------|
| `hospital.gunman.*` callouts | 1 | Terse, professional, they cover and fall back in order. Nobody makes demands. | Well-trained terrorists. | They are contractors running a field trial. |
| `hospital.gunman.overheard.01-02` | 2 | "Last crate goes up on the lift." / "Clock's running. Up, not down." | Looting before escape. | They're moving the program out from below. |
| `hospital.notice.crate`, `hospital.notice.lift` | 2 | Sealed crates on the stairs; the keyed service lift going up. | Same. | Same. |
| `hospital.notice.monitors` | 2 | Security office: every feed live except the basement's. | They cut the cameras where the hostages are. | The lab must not be on tape. |
| `hospital.nurse.02`, `hospital.civilian.03` | 1 | Gunmen came up from downstairs; they walked past civilians without looking. | Shock; terrorists after bigger targets. | They aren't taking hostages; they're clearing out from below. |
| `hospital.squad_lead.descent.02`, `hospital.notice.fire_plan` | 3 | The East Annex isn't on the city's plans; the fire plan ends at the morgue. | Old plans. | The hidden wing. |
| `hospital.chief.radio.*` | 1-4 | Every answer is "go down". He never asks about the hostages' state. | A chief under pressure. | He's herding them. |

## Chair room: second-playthrough details (beat 5)

Invisible on a first playthrough; findable if you look on a second. Build these in the room,
not in text the game shows.

- **Bodies wear SWAT gear.** Under the "wrong" silhouettes, the dead have plates, helmets and
  the same V-45s the players carry. Readable only up close and in steady light; first time
  round the flicker and bullet-time keep the player moving. (Level/art.)
- **A shoulder patch** on one body: the 14th Precinct response team patch. (Art.)
- **The shouting is their names.** `hospital.chair.vo.*` are the actual recorded words (names,
  "hold fire", "it's us"); played only through heavy distortion. Subtitles show
  `hospital.chair.shout.*` (no words). On a second listen the names can be picked out.
- **They never quite shoot back.** Muzzles up or wide; shots hit walls. (Gameplay.)
- **They reach, not attack.** Hands out, moving toward the players, not around them. (Animation.)
- **The last one** is the other player's character: its VO is the partner's voice saying the
  local player's name (`hospital.chair.vo.last.*`). Must be indistinguishable from the others
  on a first playthrough. Paid off only at the end of the game.
