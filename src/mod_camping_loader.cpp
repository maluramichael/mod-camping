/*
 * mod-camping loader.
 *
 * The playerbots fork auto-globs every module's sources into one lib and looks up a loader
 * symbol derived from the folder name: for folder "mod-camping" that symbol is exactly
 * "Addmod_campingScripts". It must exist and call our real registration function.
 *
 * Released under GNU GPL v2 or (at your option) any later version.
 */

void AddCampingScripts();

void Addmod_campingScripts()
{
    AddCampingScripts();
}
