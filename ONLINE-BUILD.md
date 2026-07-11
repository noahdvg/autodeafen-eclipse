# Build online with GitHub Actions

No command prompt or development programs are needed.

1. Sign in to GitHub and create a new empty repository.
2. Extract this ZIP using Windows' built-in **Extract All**.
3. In the repository, choose **Add file → Upload files**.
4. Upload the contents of the `autodeafen-eclipse` folder, including `.github`, `src`, `include`, `CMakeLists.txt`, and `mod.json`.
5. Commit the files.
6. Open the repository's **Actions** tab.
7. Open **Build AutoDeafen Eclipse** and choose **Run workflow**. A build also starts automatically after the upload commit.
8. Wait for the Windows build to finish.
9. Open the completed workflow run. Under **Artifacts**, download `autodeafen-eclipse`.
10. Extract that artifact ZIP. It should contain the compiled `.geode` package.
11. Rename the package to `autodeafen-eclipse.geode` only if the generated filename differs.
12. Put it in `Geometry Dash/geode/mods`, keep Eclipse installed, and restart Geometry Dash.

If the build displays a red failure, open the failed **Build Windows Geode mod** step and copy its error log back into ChatGPT. A source/API mismatch may need to be corrected before it can compile.
