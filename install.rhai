
// builds the connector from source
fn build_from_source(ctx) {
	info("Installing connector")
	let repo_path = ctx.clone_repository();
	cargo("build --release --all-features", repo_path);
	ctx.copy_cargo_plugin_artifact(repo_path, name_to_lib(ctx.crate_name()));
	info("Installing memflow kernel module")
	dkms_install(repo_path)
}

// downloads a binary release of the plugin, still needs dkms
fn install(ctx) {
	let release = ctx.github_latest_release();
	let libname = name_to_lib_with_arch(ctx.crate_name());
	let artifact = download(ctx.github_release_artifact(release, libname));
	write_plugin_artifact(artifact);
	let tarball = download(ctx.github_release_artifact(release, "memflow-" + release.semver() + "-source-only.dkms.tar.gz"));
	dkms_install(tarball);
}

