import { useState } from "react";
import "./../styles/user-menu.css";
import { LogOut } from "lucide-react";

function UserMenu({ currentUser, onSignOut }) {
	const [open, setOpen] = useState(false);

	const userName = currentUser?.name ?? "Guest";

	const initials = currentUser?.isGuest
		? "G"
		: userName.split(" ").map((part) => part[0]).join("").slice(0,2).toUpperCase();

	const userSubtitle = currentUser?.isGuest
		? "Guest session"
		: currentUser?.email;

	return (
		<div style={{ position: "relative" }}>

			<button id="user-menu-toggle" onClick={() => setOpen(true)}>
				{initials}
			</button>

			{open && (
				<>
					<div id="user-menu-backdrop" onClick={() => setOpen(false)}/>
					
					<div id="user-menu">
						{/* User info */}
						<div id="user-menu-info">
							<div id="user-initials">{initials}</div>
							
							<div style={{ minWidth: "0"}}>
								<p id="user-name">{userName}</p>
								<p id="user-email">{userSubtitle}</p>
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