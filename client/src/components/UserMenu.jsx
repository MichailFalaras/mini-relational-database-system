import { useState } from "react";
import "./../styles/user-menu.css";
import { LogOut } from "lucide-react";

function UserMenu({ onSignOut }) {
	const [open, setOpen] = useState(false);

	return (
		<div style={{ position: "relative" }}>

			<button id="user-menu-toggle" onClick={() => setOpen(true)}>
				AF
			</button>

			{open && (
				<>
					<div id="user-menu-backdrop" onClick={() => setOpen(false)}/>
					
					<div id="user-menu">
						{/* User info */}
						<div id="user-menu-info">
							<div id="user-initials">AF</div>
							
							<div style={{ minWidth: "0"}}>
								<p id="user-name">Guest</p>
								<p id="guest-email">Guest session</p>
							</div>
						</div>

						{/* Actions */}
						<div style={{ padding: "0.25rem 0" }}>
							<button 
								id="sign-out-btn"
								onClick={() => {
									setOpen(false);
									onSignOut();
								}}
							>
								<LogOut style={{ width: "0.875rem", height: "0.875rem", flexShrink: "0", color: "#9ca3af" }}/>
								Sign out
							</button>
						</div>
						
					</div>
				</>
			)}
			
		</div>
	);

}

export default UserMenu;