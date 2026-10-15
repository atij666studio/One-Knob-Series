AtiNAMatiC Sag

Power-supply sag for amp-sim rigs. When you dig in, a tube rectifier's supply
droops: the amp loses a little gain and headroom, then recovers. Sag puts that
droop-and-bloom in front of your amp model.

Signal chain position: before the amp sim.

Guitar -> [AtiNAMatiC-Sag] -> Amp Sim -> Cab/IR -> DAW

SAG     how far the supply droops when you dig in (0 = stiff, 100 = spongy)
SUPPLY  Rectifier (classic) / Variac (low voltage, softer) / Bloom (dip, then swell)
MIX     blend with the dry signal
OUTPUT  final trim

See docs/AtiNAMatiC-Sag-Manual.md for details.
