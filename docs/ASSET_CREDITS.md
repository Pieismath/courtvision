# CourtVision athlete credits

This athlete combines an original MakeHuman phenotype, original procedural tennis animation and fitted community clothing. These are actual rigged 3D assets, exported through Blender 5.2 and official MPFB 2.0.17 source; no generated concept image is used as game geometry.

| Component | Author / source | License |
| --- | --- | --- |
| Anatomical human base mesh, morph targets, game-engine skeleton | MakeHuman Community / MPFB, https://github.com/makehumancommunity/mpfb2 | CC0 1.0; license text in Licenses/MakeHuman-CC0.md |
| young_caucasian_male2 skin; high-poly eyes with brown iris; eyebrow003; eyelashes01; short04 hair | MakeHuman system assets; Data Collection AB, Joel Palmius, Jonas Hauquier and MakeHuman contributors; https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html | CC0 1.0 |
| Male polo shirt, including tailoring normal texture | Namuhekam; https://static.makehumancommunity.org/assets/assetpacks/shirts01.html | CC0 |
| elvs_male_swim_shorts1, adapted as off-white tennis shorts | Elvaerwyn; https://static.makehumancommunity.org/assets/assetpacks/pants03.html | CC-BY, as declared in original asset metadata; original notice retained in Licenses/Shorts-original-attribution.mhclo |
| TennisShoes, adapted to the athlete's shape | punkduck; https://static.makehumancommunity.org/assets/assetpacks/shoes02.html | Creative Commons Attribution 3.0, https://creativecommons.org/licenses/by/3.0/; original notice retained in Licenses/Shoes-original-attribution.mhclo |
| Crude low socks | Joel Palmius; https://static.makehumancommunity.org/assets/assetpacks/underwear04.html | CC0 |

Modifications: fitted to an original adult athlete, weight interpolation to the game-engine skeleton, subdivision and coverage-mask baking, new material colors and roughness, original ready / forehand / backhand / run animation, FBX export and Unreal material authoring. Retain the named attribution and source/license links when distributing the combined athlete. The shorts metadata declares CC-BY without a version number; this document preserves that declaration rather than assigning a new one.

MPFB's program source is GPL-3.0-or-later. Its output assets are licensed separately as above; the Blender extension is an asset-authoring tool and is not embedded in the game.

The seated spectator meshes in `../Crowd` derive from this same human and clothing. Original project modifications include seated poses, mesh simplification and randomized placement/clothing tint. The attribution and licenses above apply to these derivatives too.
