#!/bin/sh
java \
	-Dsbe.target.language=cpp98 \
	-Dsbe.output.dir=include/sbe \
	-jar third_party/simple-binary-encoding/sbe.jar \
	schema.xml
