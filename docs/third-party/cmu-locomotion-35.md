# CMU walk and run motion-capture attribution

The `locomotion_walk` and `locomotion_run` clips in
`assets/models/caligo_field_suit.*` are derived from **CMU Graphics Lab Motion
Capture Database, Subject 35, Trials 01 and 17**.

- Subject and trial descriptions:
  <https://mocap.cs.cmu.edu/search.php?subjectnumber=35>
- BVH conversion used for the Blender import:
  <https://github.com/una-dinosauria/cmu-mocap>
- Original capture: Carnegie Mellon University Graphics Lab.
- Usage statement: CMU permits the database to be used in commercial products
  but does not permit reselling the capture data directly.

The Ahamkara Blender build anchors horizontal root translation and downsamples
the source from 120 Hz to 30 Hz so the authored locomotion can loop against an
authoritative gameplay transform.
