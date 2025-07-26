import { useContext, useEffect, useLayoutEffect, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";
import { useLocation } from "preact-iso";

const Config = ({}) => {
  const { route } = useLocation();
  const { setPageTitle } = useContext(AppContext);
  useLayoutEffect(() => {
    setPageTitle('Config');
  });
  const [ config, setConfig ] = useState<string | undefined | null>();
  const reset = () => {
    setConfig(undefined);
  };
  const save = async () => {
    try {
      JSON.parse(config);
    } catch (error) {
      alert(`JSON error: ${error}`);
      return;
    }
    fetch('/api/config', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/json' }),
      body: config,
    }).catch(error => console.log(error));
  };
  const reboot = () => {
    fetch('/api/reboot', {
      method: 'GET',
    }).catch(error => console.log(error));
    route("/");
  };
  useEffect(() => {
    if (config === undefined) {
      const loadConfig = async () => {
        fetch('/api/config', {
          method: 'GET',
        }).then(response => {
          if (response.status !== 200) {
            alert(`Could not load config: ${response.status}`);
            return;
          }
          response.text().then(body => {
            setConfig(body);
          });
        }).catch(error => console.log(error));
      };
      setConfig(null);
      loadConfig();
    }
  }, [config, setConfig]);

  return (
      <div className="status-main config-main">
        <textarea
            onChange={ event => setConfig(event.target.value) }
            value={ config } />
        <p>
          <button disabled={ !config } onClick={ save }>Save</button>
          &nbsp;
          <button disabled={ !config } onClick={ reset }>Reset</button>
          &nbsp;
          <button onClick={ reboot }>Reboot</button>
        </p>
      </div>);
};

export { Config };
