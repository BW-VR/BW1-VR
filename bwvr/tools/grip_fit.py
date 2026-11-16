#!/usr/bin/env python3
"""Fit the SteamVR model of a controller on the WebXR model of it.

The mod draws the WebXR model of a controller (bwvr/assets/controllers) on
the grip pose of the runtime. SteamVR has its own model of each Quest
controller, and its component "openxr_grip" is the grip pose that SteamVR
gives. Virtual Desktop and the Oculus runtime give the same grip pose (see
docs/controller-hints.md, "9.15 The place of the controller"). This script
puts the SteamVR model in its openxr_grip frame, fits it on the WebXR model
(points on the surfaces by area, the nearest points, a rigid turn and move),
and prints the frame of the WebXR model in the grip frame. Those numbers are
the table kFits of src/mod/grip_fit_math.h.

The SteamVR models are files of SteamVR, in its folder
resources/rendermodels. The script reads them there and writes nothing.

Usage:
    python grip_fit.py WEBXR.glb STEAMVR_FOLDER
    python grip_fit.py ../assets/controllers/meta-quest-touch-plus/right.glb \
        "C:/Program Files (x86)/Steam/steamapps/common/SteamVR/resources/rendermodels/oculus_quest_plus_controller_right"

For each pair it prints:
    rms       the mean distance of the points after the fit, in mm, both ways
    q         the turn of the frame of the WebXR model in the grip frame
              (x y z w), the value of kFits
    offset    the origin of the WebXR model in the grip frame, in mm
    middle    the mean of the points on the surface of the controller, in
              the grip frame, in cm
    move      how far that middle is from the middle that the WebXR model
              draws on the grip pose, in cm

numpy and scipy are required.
"""

import json
import os
import struct
import sys

import numpy as np
from scipy.spatial import cKDTree
from scipy.spatial.transform import Rotation


def node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"]).reshape(4, 4).T
    m = np.eye(4)
    t = node.get("translation", [0, 0, 0])
    r = node.get("rotation", [0, 0, 0, 1])
    s = node.get("scale", [1, 1, 1])
    m[:3, :3] = Rotation.from_quat(r).as_matrix() @ np.diag(s)
    m[:3, 3] = t
    return m


def accessor(doc, blob, index, count):
    a = doc["accessors"][index]
    view = doc["bufferViews"][a["bufferView"]]
    start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
    kind = {5126: "f", 5125: "I", 5123: "H", 5121: "B"}[a["componentType"]]
    size = struct.calcsize(kind)
    stride = view.get("byteStride", 0) or size * count
    return np.array([struct.unpack_from("<" + kind * count, blob, start + k * stride)
                     for k in range(a["count"])])


def glb_triangles(path):
    data = open(path, "rb").read()
    length = struct.unpack("<I", data[12:16])[0]
    doc = json.loads(data[20:20 + length])
    blob = data[20 + length + 8:]
    out = []

    def walk(index, parent):
        node = doc["nodes"][index]
        world = parent @ node_matrix(node)
        if "mesh" in node:
            for p in doc["meshes"][node["mesh"]]["primitives"]:
                v = accessor(doc, blob, p["attributes"]["POSITION"], 3)
                v = v @ world[:3, :3].T + world[:3, 3]
                if "indices" in p:
                    i = accessor(doc, blob, p["indices"], 1).ravel()
                else:
                    i = np.arange(len(v))
                out.append(v[i.reshape(-1, 3)])
        for child in node.get("children", []):
            walk(child, world)

    for root in doc["scenes"][doc.get("scene", 0)]["nodes"]:
        walk(root, np.eye(4))
    return np.vstack(out)


def obj_triangles(path):
    points, faces = [], []
    for line in open(path):
        if line.startswith("v "):
            points.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("f "):
            ids = [int(x.split("/")[0]) - 1 for x in line.split()[1:]]
            for k in range(1, len(ids) - 1):
                faces.append([ids[0], ids[k], ids[k + 1]])
    return np.array(points)[np.array(faces)]


def sample(triangles, count, rng):
    area = 0.5 * np.linalg.norm(np.cross(triangles[:, 1] - triangles[:, 0],
                                         triangles[:, 2] - triangles[:, 0]), axis=1)
    pick = rng.choice(len(triangles), count, p=area / area.sum())
    r1 = np.sqrt(rng.random(count))
    r2 = rng.random(count)
    t = triangles[pick]
    return ((1 - r1)[:, None] * t[:, 0] + (r1 * (1 - r2))[:, None] * t[:, 1] +
            (r1 * r2)[:, None] * t[:, 2])


def steamvr_model(folder):
    name = os.path.basename(os.path.normpath(folder))
    text = open(os.path.join(folder, name + ".json")).read()
    doc, _ = json.JSONDecoder().raw_decode(text)
    grip = doc["components"]["openxr_grip"]["component_local"]
    return obj_triangles(os.path.join(folder, name + ".obj")), grip


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    rng = np.random.default_rng(1)
    web = sample(glb_triangles(sys.argv[1]), 40000, rng)
    triangles, grip = steamvr_model(sys.argv[2])
    raw = sample(triangles, 40000, rng)
    # The SteamVR model in its openxr_grip frame.
    turn = Rotation.from_euler("xyz", grip["rotate_xyz"], degrees=True).as_matrix()
    steam = (raw - np.array(grip["origin"])) @ turn

    tree = cKDTree(web)
    best = None
    for start in range(-40, 41, 5):
        a = Rotation.from_euler("x", start, degrees=True).as_matrix()
        t = web.mean(0) - (steam @ a.T).mean(0)
        for _ in range(80):
            p = steam @ a.T + t
            d, i = tree.query(p)
            keep = d < np.percentile(d, 95)
            pm = p[keep].mean(0)
            q = web[i][keep]
            qm = q.mean(0)
            u, _, vt = np.linalg.svd((p[keep] - pm).T @ (q - qm))
            fix = np.eye(3)
            fix[2, 2] = np.sign(np.linalg.det(vt.T @ u.T))
            r = vt.T @ fix @ u.T
            a = r @ a
            t = r @ t + qm - r @ pm
        d, _ = tree.query(steam @ a.T + t)
        rms = np.sqrt((d ** 2).mean())
        if best is None or rms < best[0]:
            best = (rms, a, t)
    rms, a, t = best
    back, _ = cKDTree(steam @ a.T + t).query(web)
    # The frame of the WebXR model in the grip frame: p_grip = q p_model + offset.
    q = Rotation.from_matrix(a.T).as_quat()
    offset = -a.T @ t
    middle = steam.mean(0)
    move = np.linalg.norm(a @ middle + t - middle)
    print("rms     %.2f mm, %.2f mm" % (rms * 1000, np.sqrt((back ** 2).mean()) * 1000))
    print("q       %.5f %.5f %.5f %.5f" % tuple(q))
    print("offset  %.1f %.1f %.1f mm" % tuple(offset * 1000))
    print("middle  %.2f %.2f %.2f cm, move %.2f cm" % (*(middle * 100), move * 100))
    return 0


if __name__ == "__main__":
    sys.exit(main())
