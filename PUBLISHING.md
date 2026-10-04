# Publish the source on GitHub

## Publication and licensing limits

The README warning must remain visible. This project has no Riot approval, and posting it does not make its use permissible. Riot's terms prohibit unauthorized tools, circumvention, and encouraging violations. A disclaimer is not permission from Riot or from third-party copyright holders.

The prepared source package contains only documentation and locally written source files. It excludes TFT APKs, libmvg, game assets, Houdini/HPE binaries, compiled helpers, screenshots, and device logs.

No redistribution license was found in the downloaded HPE repository at the pinned revision. Its public availability does not establish permission to redistribute its binaries. Do not upload the built `tft-waydroid-hpe14.zip` or attach it to a GitHub Release without verifying the relevant rights. The build script references an external repository; that is not a license grant for its contents.

No source license has been selected for this project. Choose a license for your own source before inviting reuse; MIT is one option if you want permissive reuse and have the rights to license all included source. Such a license must not claim ownership of Riot or Intel/Google binaries. See [GitHub's licensing guide](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).

## Upload through the browser

1. Sign in to GitHub and open https://github.com/new.
2. Set the repository name to `tft-waydroid-workaround`.
3. Suggested description: `Experimental TFT startup workaround for one tested Waydroid build. Account-ban risk; not approved by Riot.`
4. Select **Public** if you want it publicly accessible. Leave the README, .gitignore, and license initialization options unselected for this initial upload.
5. Click **Create repository**.
6. On the empty repository page, click **uploading an existing file**.
7. Upload all the files from the prepared source folder, including `.gitignore`. Enable hidden-file display in your file manager to see it. Put the individual files at the repository root; do not upload a ZIP as the only file.
8. Enter the commit message `Add experimental Waydroid guide and source` and click **Commit changes**.
9. Verify that the README warning appears on the repository home page and that only the expected source/documentation files are present.
10. Copy the repository URL to share it. If you select a source license, add its LICENSE file separately after reviewing its scope.

## Optional terminal upload

Create the empty GitHub repository as described above. From the prepared source folder, run the following commands. Replace YOUR_USERNAME with your GitHub username. Authenticate to GitHub when prompted; a GitHub account password is not used as a Git HTTPS password.

```bash
git init -b main
git add README.md PUBLISHING.md .gitignore build-module.py build-module.sh launch-tft.sh launch-waydroid-tft.py post-fs-data.sh tft_houdini_startup_compat.c
git commit -m "Add experimental Waydroid guide and source"
git remote add origin https://github.com/YOUR_USERNAME/tft-waydroid-workaround.git
git push -u origin main
```

See [GitHub's existing-code upload instructions](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github).
