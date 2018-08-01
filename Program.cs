using System;
using System.Collections.Generic;
using System.IO;

using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

using Microsoft.Extensions.DependencyInjection;

namespace ConsoleApplication
{
    public class Program
    {
        public static void Main(string[] args)
        {

            var config = new ConfigurationBuilder()
            .SetBasePath(Directory.GetCurrentDirectory())
            .AddJsonFile("appsettings.json")
            .AddEnvironmentVariables()
            .Build();

            //LoggerFactory loggerFactory = new LoggerFactory(config.GetSection("Logging"));
            LoggerFactory loggerFactory = new LoggerFactory();
            loggerFactory.AddConsole();
            ILogger logger = loggerFactory.CreateLogger<Program>();
            logger.LogInformation("Hello World!");

            var host = new WebHostBuilder()
            .UseConfiguration(config)
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

            })
            .ConfigureLogging((context, factory) =>
              {
                    //factory.UseConfiguration(context.Configuration.GetSection("Logging"));
                    factory.AddConfiguration(context.Configuration.GetSection("Logging"));
                    factory.AddConsole();
              })
            .UseStartup<Startup>()
            .Build();
            host.Run();
        }
    }
}
