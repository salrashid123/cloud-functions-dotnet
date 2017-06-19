DOTNETBIN=bin/mainapp

all: node_modules lib bin_from_container
	
check_deploy:
	@if ! [ -f bin/libcoreclr.so ]; then echo "Please run 'make lib' first"; exit 1; fi
	@if ! [ -f node_modules/execer/build/Release/execer.node ]; then echo "Please run 'make node_modules' first"; exit 1; fi
	@if ! [ -f bin/Release/netcoreapp2.0/ubuntu.14.04-x64/mainapp ]; then echo "Please run 'make bin_from_local' or 'make bin_from_container' first";  exit 1; fi
	@if ! [ -f bin/mainapp ]; then echo "Please run 'make bin_from_local' or 'make bin_from_container' first";  exit 1; fi
	echo "gcloud beta functions deploy YOUR_FUNCTION --stage-bucket YOUR_BUCKET --trigger-http"

bin_from_local:
	dotnet restore -r ubuntu.14.04-x64
	dotnet publish -c Release -r ubuntu.14.04-x64 -o bin/
	export LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH
	chmod u+x $(DOTNETBIN)
	@rm -rf obj/

bin_from_container:
	docker build -t dotnet_tmp -f dockerfiles/Dockerfile_dotnet .
	docker cp `docker create dotnet_tmp`:/user_code/bin .
	docker rmi -f dotnet_tmp
	@rm -rf obj/

node_modules:
	docker build -t docker_tmp -f dockerfiles/Dockerfile_node_modules .
	docker cp `docker create docker_tmp`:/user_code/node_modules .
	docker rmi -f docker_tmp

lib: 
	docker build -t docker_tmp -f dockerfiles/Dockerfile_lib .
	docker cp `docker create docker_tmp`:/user_code/lib .
	docker rmi -f docker_tmp
	
clean:
	rm -rf bin obj node_modules lib

test_with_container: lib node_modules bin_from_container
	docker build -t docker_tmp -f dockerfiles/Dockerfile_run_in_docker .
	docker run -p 8080:8080 -t docker_tmp
	#docker run -p 8080:8080 -e GOOGLE_CLOUD_PROJECT=your_project -e GOOGLE_APPLICATION_CREDENTIALS=/gcpcerts/<JSON_CERTFILE>.json  -v  /folder_with_local_cert/:/gcpcerts -t docker_tmp

test_with_local_node: bin_from_local
	npm install --save local_modules/execer
	node testserver.js

run:	
	dotnet restore
	dotnet publish -c Release -o bin/
	dotnet bin/Release/netcoreapp2.0/mainapp.dll
