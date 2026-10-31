"""Render close views of the built arms (each forearm from above and outside, each hand, the right forearm
through the eye zoomed) into /tmp/pxe-gear/views, to inspect the gear. Run from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P docs/tasks/desert-arms-gear/scripts/gear_views.py
"""
import runpy, math, os, sys, bpy
from mathutils import Vector
am = runpy.run_path("desert/tools/blender/build_arms.py", run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"](); b.build_all()
b.clear_pose()
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
sh = scene.display.shading
sh.light = "STUDIO"; sh.studio_light = "outdoor.sl"; sh.color_type = "MATERIAL"
sh.show_backface_culling = True; sh.show_object_outline = True
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w"); world.color = (0.33, 0.52, 0.71); scene.world = world
cd = bpy.data.cameras.new("c"); cd.sensor_fit = "VERTICAL"; cd.clip_start = 0.005
cam = bpy.data.objects.new("c", cd); scene.collection.objects.link(cam); scene.camera = cam
def shoot(path, eye, target, fov, res=(960, 720)):
    scene.render.resolution_x, scene.render.resolution_y = res
    cd.angle_y = math.radians(fov)
    cam.location = Vector(eye)
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Z").to_euler()
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
D = "/tmp/pxe-gear/views"
os.makedirs(D, exist_ok=True)
for S in "LR":
    sd = b.sides[S]
    c = sd.on_forearm(-0.10)
    up = sd.back
    side = sd.af.cross(sd.back).normalized()
    shoot(f"{D}/{S}_top.png", c + sd.back * 0.35 + sd.af * 0.05, c, 30)
    shoot(f"{D}/{S}_out.png", c + (side if S == "L" else -side) * 0.35 + sd.back * 0.1, c, 30)
    h = sd.W + sd.f * 0.04
    shoot(f"{D}/{S}_hand.png", h + sd.n * 0.25 + sd.f * -0.05, h, 25)
shoot(f"{D}/eye_zoom_R.png", (0, 0, 0), b.sides["R"].on_forearm(-0.05), 22)
