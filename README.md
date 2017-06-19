
# .NET on Google Cloud Functions

_Disclaimer: This is not an official Google product. It is not and will not be maintained by Google, and is not part of Google Cloud Functions project. There is no guarantee of any kind, including that it will work or continue to work, or that it will supported in any way._

Sample application running .NET [Cloud Function](https://cloud.google.com/functions/docs/) as an [HTTP Trigger](https://cloud.google.com/functions/docs/calling/http).  

This is a **unofficial** port to .NET of cloud-functions-go sample here:
- [https://github.com/GoogleCloudPlatform/cloud-functions-go](https://github.com/GoogleCloudPlatform/cloud-functions-go)


## Prerequsites

This script requires docker mulit-stage builds provided docker-ce 17.05+ at a minimum.  You can optionally compile and run the node_module if you have the 
prerequsites specified in cloud-fucntions-go.

Optionally, you can also install dotnet 2.0.0+ on the workstation to build and run the sample directly.  You can also run the sample within docker with dotnet-runtime.

* docker-ce 17.05.0+
  - [https://download.docker.com/linux/debian/dists/jessie/pool/edge/amd64/docker-ce_17.05.0~ce-0~debian-jessie_amd64.deb](https://download.docker.com/linux/debian/dists/jessie/pool/edge/amd64/docker-ce_17.05.0~ce-0~debian-jessie_amd64.deb)
* dotnet 2.0.0-preview1
  - Either install or use binaries:
  - [https://github.com/dotnet/core/blob/master/release-notes/download-archives/2.0.0-preview1-download.mdg](https://github.com/dotnet/core/blob/master/release-notes/download-archives/2.0.0-preview1-download.mdg)


## Makefile

| Rule  | Action |
| ------------- | ------------- |
| all  | builds everything and prepares the current folder to deploy to GCF  |
| check_deploy  | verifies the current folder is staged for deployment  |
| run  | build and run the core .NET executeable for the **local** platform  |
| bin_from_local  | builds the .NET executeable specifically for ubuntu.14.04-x64 and copies the binary to bin/mainapp.  |
| bin_from_container  | builds the .NET executable file (bin/mainapp) within a container and copies out the bin/ folder  |
| lib  | copies the .so files required by dotnet to the lib/ folder  |
| node\_modules  | Builds the node_module/ folder and execer.cc binary  |
| test_with_container  | build and run everything within a container.   |
| test_with_local_node  | launches the node test webapp on :8080 and then passes the file descriptors to .NET via local_modules/execer/execer.cc  |
| clean  | deletes all generated files  |

Recommended to use [Cygwin](https://www.cygwin.com/) on windows with (make|zip)

## Writing a cloud function and running locally

Edit Startup.cs file to add on your HTTP Trigger function to the following endpoint:

Startup.cs


```csharp

        private static void Req(IApplicationBuilder app)
        { 
            app.Run(async context =>
            {   
                _logger.LogInformation("HTTP handler called..");  
                await context.Response.WriteAsync("hello from .NET");
            });
        }  
```

then run the dotnet application as normal:

```
dotnet restore
dotnet run
```

To simulate deployment and the nodeJS module passing control, run

```
$ make test_with_container
```

which does the following:

1. Runs temp container to acquire the .so files needed for .NET and copies them to lib/
2. Runs temp container to build the .NET executable and copies them to bin/
3. Runs temp container to build node\_modules and copies them to the hosts node_modules/ folder
4. Launches node webserver within a container and listens on :8080
5. Passes the socket file descriptors from node to .NET
6. .NET takes over the sockets from Node.

> Note: [Application Default Credentials](https://developers.google.com/identity/protocols/application-default-credentials) will not work 
unless you pass though GOOGLE_CLOUD_PROJECT and _GOOGLE_APPLICATION_CREDENTIALS_ env variable plus the json certificte file (shown in the Makefile)

## Deploying

If you have coreclr/dotnet installed locally, build all the dependencies

```
make node_modules lib bin_from_local
```

if not, build the .NET cloud function executeable inside a container:

```
make node_modules lib bin_from_container
```

then check if all the necessary files exist to deploy
```
$ make check_deploy
```

Finally, upload as an HTTP Trigger as described here:

- [https://cloud.google.com/functions/docs/deploying/filesystem](https://cloud.google.com/functions/docs/deploying/filesystem)

```
gcloud beta functions deploy gcfdotnet --stage-bucket your_staging_bucket --trigger-http
```

> Note:  it takes quite sometime to generate all the lib/ and node_modules/ dependencies so once they are already built using 'make all' you can
selectively recreate the .NET binary using 'make bin_from_local' or 'make bin_from_container' and directly deploy using gcloud.

## Appendix

### Required libraries for dotnet

- dotnet runtime requires the following libraries.  You can use this to install runtime support on debian without the full dotnet install [microsoft/dotnet:2.0.0-preview1-runtime-jessie](https://github.com/dotnet/dotnet-docker/blob/master/2.0/sdk/jessie/Dockerfile#L4)


### Misc links

The following snippets and links describes how control transfer for the socket file descriptors takes place from node->cpp->.NET

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

Then in .NET, use the following Kestrel options to listen on a handler

* https://github.com/aspnet/KestrelHttpServer/blob/dev/src/Microsoft.AspNetCore.Server.Kestrel.Core/KestrelServerOptions.cs


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