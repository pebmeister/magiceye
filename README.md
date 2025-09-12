Create stereogram

Usage: magic_eye input.stl texture.png/null outprefix [options]
Options:
  -w width             : Output width (default: 1200)
  -h height            : Output height (default: 800)
  -sep eye_sep         : Eye separation in pixels (default: 100)
  -fov fov_deg         : Field of view in degrees (default: 45)
  -persp 0|1           : 1 for perspective, 0 for orthographic (default: 1)
  -cam x,y,z           : Camera position (default: auto)
  -look x,y,z          : Look-at point (default: auto)
  -rot x,y,z           : Rotate model (degrees, XYZ order default: 0,0,0)
  -trans x,y,z         : Translate model (default: 0,0,0)
  -sc x,y,z            : Scale model ( default :1,1,1)
  -orthsc              : Orthographic scale (default : 1)
  -sepbg               : background seperation scale (default :0.6)
  -depthgama depth     : depth gama adjust (default: 0.95)
  -orthtune lo hi      : Orthographic scale tuning lo hi (default: 0.6 1.2)
  -shear x,y,z         : Shear model (XY,XZ,YZ  default: 0,0,0)
  -depthrange near far : Set normalized depth range (default: 0.75 0.75
  -brightness val      : Texture brightness (0.5-2.0, default: 1)
  -contrast val        : Texture contrast (0.5-2.0, default: 1)
  -fthresh thresh      : Forground threshhold (0-1 default: 0.9)
  -sthresh thresh      : Smooth threshhold (0-1 default  0.75)
  -sweight weight      : Smooth weigth (default: 10)
  -laplace             : Enable Laplace mesh smoothing (default: 0)
  -laplacelayers       : Laplace smooth layers (if laplace enabled default: 15)
  -rwidth              : Ramp width (default: 2.5)
  -rheight             : Ramp height (default: 100)

