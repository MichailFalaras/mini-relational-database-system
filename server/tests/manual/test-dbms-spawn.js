import { DBMSProcessManager } from "../../src/dbms/DBMSProcessManager.js";

const manager = new DBMSProcessManager("./tests/fixtures/mock-dbms/mock_dbms.exe");

await manager.start();

console.log("Spawn requested");

setTimeout(() => {
	// Note - '\n' doesn't close the communication channel
	manager.child.stdin.write("Hello from Node.js\n");
}, 1000);

setTimeout(() => {
	// Close the writable end of the stdin pipe, allowing the C process to detect EOF
	manager.child.stdin.end();
}, 2000);