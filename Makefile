DEPS_BUILD_DIR=.deps
DEPS_SOURCE_DIR=deps

debug:
	@cmake -H. -GNinja \
		-Bvar/build/native \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DCMAKE_INSTALL_PREFIX=${DEPS_BUILD_DIR}/usr
	@cmake --build var/build/native

third-party:
	@cmake -GNinja -H${DEPS_SOURCE_DIR} -B${DEPS_BUILD_DIR} \
		-DCMAKE_BUILD_TYPE=Release \
		-DUSE_BUNDLED_BOOST=OFF
	@cmake --build ${DEPS_BUILD_DIR}

docker-third-party:
	@cmake -GNinja -H${DEPS_SOURCE_DIR} -B${DEPS_BUILD_DIR} -DCMAKE_BUILD_TYPE=Release
	@cmake --build ${DEPS_BUILD_DIR}

ci:
	cmake -H${DEPS_SOURCE_DIR} -B${DEPS_BUILD_DIR} -DCMAKE_BUILD_TYPE=Release
	cmake --build ${DEPS_BUILD_DIR}
	cmake -H. -Bvar/build/native -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build var/build/native

release:
	@cmake -H. -GNinja \
		-Bvar/build/native \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=${DEPS_BUILD_DIR}/usr
	@cmake --build var/build/native

linux-install:
	cp -f var/build/appimage/komai-latest-x86_64.AppImage ~/.local/bin/komai.AppImage

macos-app-install:
	cp -Rf var/build/native/komai.app /Applications

lint:
	just lint

license:
	./.ci/licenses.sh

linux-deploy:
	./.ci/linux/deploy.sh

macos-deploy:
	./.ci/macos/deploy.sh

update-translations:
	lupdate \
		-locations relative \
		-Iinclude/dialogs \
		-Iinclude \
		src/ resources/qml/ -ts resources/langs/nheko_*.ts -no-obsolete

clean:
	rm -rf var/build/native
