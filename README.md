# EvaluPoint 1

EvaluPoint checks measured 3D points against a geometric form and a tolerance. It fits the shape, measures the form error, and marks each point inside or outside the tolerance zone.

The program is a small GD&T (geometric dimensioning and tolerancing) viewer for point clouds from a part measurement. It is meant to show whether a surface stays within the allowed form, and where the points that fall outside the zone are.

## What it checks

| Form | Check | Meaning |
| --- | --- | --- |
| Cylinder | Cylindricity | Gap between the smallest and largest radius about the fitted axis |
| Square plate | Flatness | Gap between two parallel planes that enclose the plate |
| Sphere | Sphericity | Gap between the smallest and largest concentric spheres |
| Cone | Cone form | Gap between two coaxial cones, measured perpendicular to the surface |

A check passes when the form error is within the tolerance you set. Points inside the zone are accepted. Points outside it are listed and highlighted.

## Point files

Each line is one point: `x y z`. Commas are treated as spaces. Blank lines and lines that start with `#` are skipped.

Sample clouds are in `samples/`:

- `cylinder.txt`
- `square.txt`
- `sphere.txt`
- `cone.txt`

## How to run

Open the viewer with the sample cylinder:
