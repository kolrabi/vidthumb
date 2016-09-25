# vidthumb

A thumbnail generator for video files (and zip files containing images, but mostly videos).

It will take a video file, scan it for "interesting" frames based on a totally unscientific 
heuristic and create an overview image containing thumbnails of said frames.

## Syntax

vidthumb [-p] videoFile output.png

The optional -p switch selects a portrait aspect ratio for the overview image.

