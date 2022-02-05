@pushd %~dp0
pandoc --shift-heading-level-by=-1 -s README.md -o docs/index.html
copy /y screenshot.png docs
@popd
