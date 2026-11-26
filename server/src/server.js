import app from "./app.js";
import config from "./config/index.js";
import { DBMSProcessManager } from "./dbms/DBMSProcessManager.js";

const dbmsManager = new DBMSProcessManager(config.dbmsExecutablePath);

let server = null;
let isShuttingDown = false;


/* ---------- BACKEND STARTUP ---------- */

async function start() {
    try {
        // Initialize the C DBMS subprocess
        const startupPromise = dbmsManager.start();

		// Testing: Send message to parent process if the current processed is spawned with an IPC
		if (config.nodeEnv === "test" && process.connected) {
            const child = dbmsManager.child;

            if (child?.pid) {
                process.send({
                    type: "dbms-spawned",
                    pid: child.pid
                });
            }

            child?.once("close", () => {
                if (process.connected) {
                    process.send({
                        type: "dbms-closed",
                        state: dbmsManager.state
                    });
                }
            });
        }

		await startupPromise;

        // Prevent HTTP startup if shutdown has already begun
        if (isShuttingDown) {
            return;
        }

        console.log("BaseQL DBMS is ready.");

        // Initialize the HTTP server
        server = app.listen(config.port, config.host);

        // Handle successful HTTP server startup
        server.once("listening", () => {
            console.log(
                `BaseQL backend running at http://${config.host}:${config.port}`
            );

            if (config.nodeEnv === "test" && process.connected) {
                process.send({
                    type: "backend-ready",
                    port: server.address().port
                });
            }
        });

        server.on("error", (error) => {
            console.error("Unable to start BaseQL backend:", error);
            process.exitCode = 1;

            void shutdown("HTTP server error");
        });

    } catch (error) {
        // Shutdown may have intentionally interrupted startup
        if (isShuttingDown) {
            return;
        }

        console.error("Unable to start BaseQL backend:", error);
        process.exitCode = 1;

        await shutdown("Backend startup failure");
    }
}

/* ---------- HTTP SERVER CLEANUP ---------- */

async function closeHTTPServer() {
    if (server === null) {
        return;
    }

    await new Promise((resolve, reject) => {
        server.close((error) => {
            if (error && error.code !== "ERR_SERVER_NOT_RUNNING") {
                reject(error);
            } else {
                resolve();
            }
        });
    });
}

/* ---------- FORCED DBMS TERMINATION ---------- */

async function forceTerminateDBMS(error) {
    const child = dbmsManager.child;

    if (child === null) {
        return;
    }

    // Register the listener before terminating the process
    const closed = new Promise((resolve) => {
        child.once("close", resolve);
    });

    // Reject pending operations and mark the process as failed
    dbmsManager.failProcess(error);

    // Terminate the child process
    dbmsManager.terminateFailedProcess(child);

    // Wait for the child process to close
    await closed;
}

/* ---------- DBMS CLEANUP ---------- */

async function closeDBMS() {
    if (dbmsManager.child === null) {
        return;
    }

    try {
        await dbmsManager.shutdown();

        console.log("BaseQL DBMS shut down gracefully.");
    } catch (error) {
        console.error("Error during DBMS shutdown:", error);
        process.exitCode = 1;

        await forceTerminateDBMS(error);
    }
}

/* ---------- BACKEND SHUTDOWN ---------- */

async function shutdown(signal) {
    if (isShuttingDown) {
        return;
    }

    isShuttingDown = true;

    console.log(`Received ${signal}. Shutting down BaseQL backend...`);

    // Prevent shutdown from hanging indefinitely
    const shutdownTimeout = setTimeout(async () => {
        console.error("Backend shutdown timed out.");

        // Bound emergency cleanup
        const forcedExitTimeout = setTimeout(() => {
            console.error("Forced DBMS cleanup timed out.");
            process.exit(1);
        }, 5000);

        try {
            await forceTerminateDBMS(new Error("Backend shutdown timed out"));
        } catch (error) {
            console.error("Forced DBMS cleanup failed:", error);
        } finally {
            clearTimeout(forcedExitTimeout);
            process.exit(1);
        }

    }, 30000);

    try {
        // Stop accepting HTTP connections and finish active requests
        await closeHTTPServer();

    } catch (error) {
        console.error("Error during HTTP server shutdown:", error);
        process.exitCode = 1;
    }

    try {
        // Shut down the DBMS even if HTTP shutdown failed
        await closeDBMS();

    } catch (error) {
        console.error("Error during DBMS cleanup:", error);
        process.exitCode = 1;

        // Keep the overall timeout active if a child remains
        if (dbmsManager.child !== null) {
            return;
        }
    }

    clearTimeout(shutdownTimeout);

    if (!process.exitCode) {
        console.log("BaseQL backend shut down gracefully.");
    }

	// Testing: Close the IPC channel to the parent process, so that the child process exits gracefully 
	if (process.connected) {
		process.disconnect();
	}
}


/* ---------- SIGNAL HANDLERS ---------- */

process.on("SIGINT", () => void shutdown("SIGINT"));
process.on("SIGTERM", () => void shutdown("SIGTERM"));

// Testing: Add event listener so that the spawned process can receive messages from the testing process
if (config.nodeEnv === "test" && process.connected) {
	process.on("message", (message) => {
		if (message?.type === "shutdown") {
			void shutdown("IPC shutdown request");
		}
	});
}
 
/* ---------- APPLICATION ENTRY POINT ---------- */

await start();