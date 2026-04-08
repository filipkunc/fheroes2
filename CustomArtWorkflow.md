# Custom Art Workflow

## General Notes

What we found actually works:
  1. Use Gemini to generate art together with multiple original images and reference art from previous attempt.
  2. Critical is to use 3x3 grid or similar to ensure the art is consistent in the animation and also system prompt helps with that.

It looks to me the Python scripts which do slicing and background removal are mostly broken, because they are too simplistic and rely on light grey background removal which very often collides with generated art.
Shadow handling is also very often questionable and better approach is needed.
Currently multiple rounds + manual background removal using Krita and the contiguous selection tool is the way to go.
Main problem seems to be when Claude drives this it does not have the same visual understanding as the latest Gemini models which can do very consistent art. The way to go is to rely a bit more on the human in the loop to make proper decisions.

## Tooling

I definitely miss a preview and app/tool helping me driving this process, pure prompting is just insane because it misses the visual feedback I need and also makes it harder to use a tool like Krita to nail down the quality.
Additionaly it looks Claude is constantly trying to understand and relearn how the game art works, custom tool which would allow to preview and modify the game animations, sprite sheets would make this faster.
I also need to know what is sent to Gemini before that is done to keep the cost in check.

## High Resolution RGBA assets

The original game was created in a time when only palette 256 colors out of 24 bit color space was possible to keep performance in check.
Nowadays this recreation of the game from scratch somehow keeps this limitation but uses upscaling and actually blits the final image to real 32 bit color buffer.
There is no need for this complexity, we need to make sure the custom art is displayed with the best possible quality to do that we need to load the original high resolution image with full color and transparency in 32 bit and then handle the
additional upscaling or downscaling in the engine.
Easiest might be to find where we really need the palette handling and switch to full color with alpha (RGBA). I remember doing something similar in the past and it was not that hard.
Additional debugging and logging of the rendering flow might help, we might also need a bit of tooling which for example intercepts the calls via wrapper over SDL and the engine functions to be able to debug if the sprites are rendered correctly.
In the worst case multiple layers can be used, but I do not think that is necessary, because only complexity might be palette cycling which is technically just a color replacement on the fly which can be done with a shader too.
