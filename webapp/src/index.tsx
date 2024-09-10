import { createContext, render } from 'preact';
import { LocationProvider, Router, Route } from 'preact-iso';

import './style.css';
import { Status } from "./pages/status";
import { NotFound } from "./pages/not-found";
import { Header } from "./header";
import { useMemo, useState } from "react";

interface AppContextValue {
	pageTitle: string;
	setPageTitle: (title: string) => void;
}

export const AppContext = createContext<AppContextValue>({} as AppContextValue);

export const App = () => {
	const [ pageTitle, setPageTitle ] = useState('Main');
	const appContext = useMemo(() => {
		return { pageTitle, setPageTitle };
	}, [ pageTitle ]);

	return (
			<AppContext.Provider value={ appContext }>
				<LocationProvider>
					<Header />
					<main>
						<Router>
							<Route path="/" component={ Status } />
							<Route default component={ NotFound } />
						</Router>
					</main>
				</LocationProvider>
			</AppContext.Provider>
	);
}

render(<App/>, document.getElementById('app'));
