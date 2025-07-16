import { createContext, render } from 'preact';
import { LocationProvider, Router, Route } from 'preact-iso';

import './style.css';
import { Status } from "./pages/status";
import { NotFound } from "./pages/not-found";
import { Header } from "./header";
import { useMemo, useState } from "react";
import { Config } from "./pages/config";

interface AppContextValue {
	pageTitle: string;
	setPageTitle: (title: string) => void;
}

export const AppContext = createContext<AppContextValue>({} as AppContextValue);

const Main = () => (
		<>
			<Header />
			<main>
				<Router>
					<Status path="/" />
					<Config path="/config" />
					<NotFound default />
				</Router>
			</main>
		</>);

export const App = () => {
	const [ pageTitle, setPageTitle ] = useState('Main');
	const appContext = useMemo(() => {
		return { pageTitle, setPageTitle };
	}, [ pageTitle ]);

	return (
			<AppContext.Provider value={ appContext }>
				<LocationProvider>
					<Router>
						<Main path="/*" />
						<Main default />
					</Router>
				</LocationProvider>
			</AppContext.Provider>
	);
}

render(<App/>, document.getElementById('app'));
