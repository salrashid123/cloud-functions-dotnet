
# .NET on Google Cloud Functions

_Disclaimer: This is not an official Google product. It is not and will not be maintained by Google, and is not part of Google Cloud Functions project. There is no guarantee of any kind, including that it will work or continue to work, or that it will supported in any way._

This is a sample application running a .NET [Cloud Function](https://cloud.google.com/functions/docs/) as an [HTTP Trigger](https://cloud.google.com/functions/docs/calling/http).  

This is a **unofficial** port to .NET of cloud-functions-go sample here:
- [https://github.com/GoogleCloudPlatform/cloud-functions-go](https://github.com/GoogleCloudPlatform/cloud-functions-go)

---

## Prerequsites

There are several ways you can build and run these samples but all you really need is docker 17

If you have dotnet SDK installed, it'll become easier to develop and test.

---

## Quickstart:  Build+Run with Cloud Shell

This is the easiest way to try this out is with Google Cloud Shell as it already has
docker 17+, node and dotnet2.0+

1. [Enable Cloud Functions API](https://cloud.google.com/functions/docs/quickstart) on your project.
2. Open up [Cloud Shell](https://cloud.google.com/shell/docs/starting-cloud-shell)
3. Get the repo
```
     git clone https://github.com/salrashid123/cloud-functions-dotnet.git
```
4. Build and stage the files:
```
     cd cloud-functions-dotnet

     docker build -t docker_tmp .

     docker cp `docker create docker_tmp`:/user_code/lib .
     docker cp `docker create docker_tmp`:/user_code/bin .
     docker cp `docker create docker_tmp`:/user_code/node_modules .
```

5. Deploy
Remember to specify a staging GCS bucket (any bucket you have read-write permissions).
```
     gcloud beta functions deploy gcfdotnet --stage-bucket your_staging_bucket --trigger-http
```
6. Verify Deployment by invoking the endpoint URL (this may take sometime to initialize first time ~mins)
```
     curl -v https://us-central1-your_project.cloudfunctions.net/gcfdotnet
```

Thats it!  The following section describes how this works and how to build if you have dotnet locally

This sample uses Docker [multi-stage builds](https://medium.com/@salmaan.rashid/multi-stage-docker-builds-with-net-ef66fc98b51d) compile nodeJS mdoules
and dotnet, then finally acquire the requisite linux shared_objects to run on Linux.   Once all those files are ready within the container,  the last set
of steps 'copies' them out of the container so you can deploy the function directly.

Note, if you build the full Default dockerfile, you can execute the container since
the build step includes this by default:
```
     docker run -p 8080:8080 -t docker_tmp
```

---

## Writing a cloud function

You need to have dotnet 2.0 available:

Edit Startup.cs file to add on your HTTP Trigger function here.:

Startup.cs
```csharp

        private static void Execute(IApplicationBuilder app)
        {
            app.Run(async context =>
            {   
                _logger.LogInformation("HTTP handler called..");  
                await context.Response.WriteAsync("hello from .NET");
            });
        }  
```

At this point, you can build and deploy with docker as above.

## Building dotnet bin/ folder locally

If you have dotnet2.0 installed locally, you can build and run it locally to to test

```
     dotnet restore -r ubuntu.14.04-x64
     dotnet publish -c Release -r ubuntu.14.04-x64 -o bin/
```

then set the permissions before deploying and executing:

```
    chmod u+x bin/mainapp
    bin/mainapp
```

The entrypoint for your cloud funtion must be '/execute'

```
    http://localhost:8080/execute
```

Note that if you wish to develop locally and have dotnet installed, you can modify the default
Docker file to *not* build the dotnet binary:

```dockerfile
FROM node:8.9.1 AS build-env-node
ADD . /user_code
WORKDIR /user_code
RUN npm install --save local_modules/execer

FROM microsoft/dotnet:2.0.0-sdk AS build-env-dotnet
ADD . /user_code
WORKDIR /user_code
RUN mkdir -p /user_code/lib && \
    for i in `dpkg -L libc6 libcurl3 libgcc1 libgssapi-krb5-2 libicu57 liblttng-ust0 libssl1.0.2 libstdc++6 libunwind8 libuuid1 zlib1g  | egrep "^/usr/lib/x86_64-linux-gnu/.*\.so\."`; do cp $i /user_code/lib; done

FROM gcr.io/distroless/dotnet
WORKDIR /user_code
ADD . /user_code
COPY --from=build-env-node /user_code/node_modules /user_code/node_modules
COPY --from=build-env-dotnet /user_code/lib /user_code/lib
```

but build the other components with docker:
```
 docker build -t docker_tmp .

 docker cp `docker create docker_tmp`:/user_code/lib .
 docker cp `docker create docker_tmp`:/user_code/node_modules .
```

After that, build dotnet locally
```
dotnet restore -r ubuntu.14.04-x64
dotnet publish -c Release -r ubuntu.14.04-x64 -o bin/
chmod u+x bin/mainapp
```

Then just deploy as shown above


## Appendix


### Required libraries for dotnet

- dotnet runtime requires the following libraries.  You can use this to install runtime support on debian without the full dotnet install [microsoft/dotnet:2.0.0-runtime-jessie](https://github.com/dotnet/dotnet-docker/blob/master/2.0/sdk/jessie/Dockerfile#L4)

Essentially this is a copy of the required minimum set needed for dotnet to run on the OS wihtout extras.  For more information on this,
see [distroless builds](https://github.com/GoogleCloudPlatform/distroless)

### How this application works

The following snippets and links describes how control transfer for the socket file descriptors takes place from

```
node -> cpp -> .NET
```

Acquire the file descriptors here by modifying the following to pass in LD\_LIBRARY_PATH environment variables:

* https://github.com/GoogleCloudPlatform/cloud-functions-go/blob/master/local_modules/execer/execer.cc#L77

change to:

```cc
	const char bin[] = "./bin/mainapp";
	char *envp[] =
	{
			"LD_LIBRARY_PATH=/user_code/lib:$LD_LIBRARY_PATH",
			NULL
	};

	execve(bin, const_cast<char* const*>(&args[0]), envp);    

```

Then in .NET, use the following [Kestrel Options](https://github.com/aspnet/KestrelHttpServer/blob/dev/src/Microsoft.AspNetCore.Server.Kestrel.Core/KestrelServerOptions.cs) to listen on a handler.


Program.cs

```csharp

            .UseKestrel(options =>
            {
                try
                {
                    for (int i = 0; i < args.Length; i++)
                    {
                        string value = args[i];
                        if (value == "--fds")
                        {
                            var fds = args[i+1].Split(",")[0];
                            string env_var = Environment.GetEnvironmentVariable("LD_LIBRARY_PATH");
                            logger.LogDebug("Using LD_LIBRARY_PATH: " + env_var);
                            logger.LogDebug("Setting kestrel ListenHandler to fd >>>>>>>> " + fds);
                            ulong fd = Convert.ToUInt64(fds);
                            options.ListenHandle(fd);
                            break;                     
                        }                      
                    }       
                }
                catch (System.IndexOutOfRangeException e)
                {
                    logger.LogError("Provided filedescriptor argument but unable to parse descriptor list " + e);
                }
                catch (Exception ex) {
                    logger.LogCritical(ex.Message);                    
                }
```
