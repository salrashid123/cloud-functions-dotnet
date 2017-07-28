using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Logging;

namespace ConsoleApplication {

    public class Startup {

        private static ILogger _logger;
        private static void Execute(IApplicationBuilder app)
        { 
            app.Run(async context =>
            {   
                _logger.LogInformation("HTTP handler called..");  
                await context.Response.WriteAsync("hello from .NET");
            });
        }        

        private static void Ready(IApplicationBuilder app)
        {
            app.Run(async context =>
            {
                await context.Response.WriteAsync("User function is ready");
            });
        }

        private static void Init(IApplicationBuilder app)
        {
            app.Run(async context =>
            {
                await context.Response.WriteAsync("ok");
            });
        }

        public void Configure(IApplicationBuilder app, IHostingEnvironment env,  ILoggerFactory loggerFactory)
        {       
           _logger = loggerFactory.CreateLogger<Startup>(); 
           app.Map("/start", Ready);   
           app.Map("/check", Ready);
           app.Map("/init", Init);
           app.Map("/execute", Execute);           
        }
    }
}
