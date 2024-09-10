import './style.css';
import { useContext } from "react";
import { AppContext } from "../index";
import { MenuIcon } from "../icons";

const Header = () => {
  const { pageTitle } = useContext(AppContext);
  return (
      <header>
        <div class="title">{ pageTitle }</div>
        <div class="menu-icon">
          <MenuIcon fillColor={ 'white' } />
        </div>
      </header>
  );
}

export { Header };
