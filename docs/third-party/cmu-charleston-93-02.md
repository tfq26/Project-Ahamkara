# CMU Charleston motion-capture attribution

The animation clip `dance_charleston` in `assets/models/caligo_field_suit.*`
is derived from **CMU Graphics Lab Motion Capture Database, Subject 93,
Trial 02 (Charleston dance)**.

- Source: <https://mocap.cs.cmu.edu/subjects/93/>
- BVH conversion used for the Blender import:
  <https://github.com/una-dinosauria/cmu-mocap>
- Original capture: Carnegie Mellon University Graphics Lab.
- Usage statement: CMU permits the database to be used in commercial products
  but does not permit reselling the capture data directly. The BVH converter
  adds no additional usage restrictions.

The Ahamkara build script anchors horizontal root translation so the clip can
loop safely in a gameplay character showcase. It also downsamples export from
120 Hz to 30 Hz; the performed motion itself is otherwise preserved.
