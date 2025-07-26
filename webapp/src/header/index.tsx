import './style.css';
import { useContext } from "react";
import { AppContext } from "../index";
import { ConfigIcon, MenuIcon } from "../icons";
import { useLocation, useRoute } from "preact-iso";

const Header = () => {
  const { path } = useRoute();
  const { route } = useLocation();
  const { pageTitle } = useContext(AppContext);
  const jumpToPage = (page: 'main' | 'config') => {
    if (page === "main") {
      route("/");
    } else if (page === "config") {
      route("/config");
    }
  };
  return (
      <header>
        <div class="title">{ pageTitle }</div>
        <div class="menu-icon" onClick={ () => jumpToPage(path === '/' ? 'config' : 'main') }>
          {
            path === '/'
              ? <ConfigIcon fillColor={ 'white' } />
              : <MenuIcon fillColor={ 'white' } />
          }
        </div>
      </header>
  );
}

export { Header };
