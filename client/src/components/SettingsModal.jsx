import { X } from "lucide-react";
import "./../styles/settings-modal.css";


// Available configurable setting values
const FONT_OPTIONS = [12, 13, 14, 15];
const TAB_WIDTH_OPTIONS = [2, 4];
const LIMIT_OPTIONS = [25, 50, 100, 500, 1000];
const TIMEOUT_OPTIONS = [10, 30, 60, 120];

//: 100,
       
        //tabWidth: 2,
        //: 30


function SettingsModal({settings, setSettings, onClose}) {
	
	return (
		<div id="settings-modal-backdrop">
			<div id="settings-modal">

				{/* Header */}
				<div id="settings-modal-header">
					<div>
						<h2>Settings</h2>
						<p>Preferences take effect immediately</p>
					</div>

					<button id="close-settings-modal" onClick={onClose}>
						<X style={{width: "1rem", height: "1rem" }}/>
					</button>
				</div>

				<div id="settings-modal-editor">

					{/* Editor Section */}
					<div>
						<p className="section-title">Editor</p>
						<div className="settings">

							<div className="setting">
								<div>
									<p className="setting-title">Font size</p>
									<p className="setting-description">Size of the SQL editor text</p>
								</div>

								<div className="options-container">
									{FONT_OPTIONS.map((option) => {
										const isActive = option === settings?.editorFontSize;

										return (
											<button 
												key={option} 
												className={`setting-option ${isActive ? "active" : ""}`}
												onClick={() => 
													setSettings((prev) => ({ 
														...prev, 
														editorFontSize: option
													}))
												}
											>
												{option}px
											</button>
										)
									})}
								</div>
							</div>

							<div className="setting">
								<div>
									<p className="setting-title">Tab width</p>
									<p className="setting-description">Spaces inserted by the Tab key</p>
								</div>

								<div className="options-container">
									{TAB_WIDTH_OPTIONS.map((option) => {
										const isActive = option === settings?.tabWidth;

										return (
											<button 
												key={option} 
												className={`setting-option ${isActive ? "active" : ""}`}
												onClick={() => 
													setSettings((prev) => ({ 
														...prev, 
														tabWidth: option
													}))
												}
											>
												{option}px
											</button>
										)
									})}
								</div>
							</div>
						</div>
					</div>
					
					<div style={{ height: 1, background: "rgba(0,0,0,0.07)" }} />

					{/* Query Section */}
					<div>
						<p className="section-title">Query</p>
						<div className="settings">

							<div className="setting">
								<div>
									<p className="setting-title">Default row limit</p>
									<p className="setting-description">Applied when browsing a table</p>
								</div>

								<select
									className="setting-dropdown"
									value={settings?.defaultLimit}
									onChange={(event) => 
										setSettings((prev) => ({
											...prev,
											defaultLimit: parseInt(event.target.value)
										}))
									}
								>
									{LIMIT_OPTIONS.map(limit => <option key={limit} value={limit}>{limit} rows</option>)}
								</select>
							</div>

							<div className="setting">
								<div>
									<p className="setting-title">Query timeout</p>
									<p className="setting-description">Maximum execution time per query</p>
								</div>

								<select
									className="setting-dropdown"
									value={settings?.queryTimeout}
									onChange={(event) => 
										setSettings((prev) => ({
											...prev,
											queryTimeout: parseInt(event.target.value)
										}))
									}
								>
									{TIMEOUT_OPTIONS.map(timeout => <option key={timeout} value={timeout}>{timeout}s</option>)}
								</select>
							</div>
						</div>
					</div>
				</div>
				
				{/* Footer */}
				<div id="settings-modal-footer">
					<button onClick={onClose}>
						Done
					</button>
				</div>
			</div>
		</div>
	);
}

export default SettingsModal;