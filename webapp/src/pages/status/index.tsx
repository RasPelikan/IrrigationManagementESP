import { useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";

interface ImStatus {
  rssi: number;
  waterLevel: number;
  waterPressure: number;
  irrigationPump: 'active' | 'inactive' | 'out-of-water';
  irrigationPumpMode: 'active' | 'inactive' | 'auto';
  wellPump: 'active-cycle' | 'inactive-cycle' | 'inactive';
  wellPumpMode: 'on' | 'off' | 'auto';
  wellPumpCycle: number | undefined;
}

const Status = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  useLayoutEffect(() => {
    setPageTitle('Status');
  });

  const [ currentDate, setCurrentDate ] = useState<Date | undefined>(undefined);
  const timerRef = useRef<number>(-1);
  const [ status, setStatus ] = useState<ImStatus>({
    irrigationPump: "inactive",
    irrigationPumpMode: "auto",
    wellPump: "inactive",
    wellPumpMode: "auto",
  });

  const [ connected, setConnected ] = useState(false);
  const eventSource = useRef<EventSource | undefined>(undefined);
  useEffect(() => {
    eventSource.current = new EventSource('/api/status-events');
    eventSource.current.onopen = () => {
      console.log("Events Connected");
      setConnected(true);
    };
    eventSource.current.onerror = event => {
      // @ts-ignore
      if (event.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
        setConnected(false);
      } else {
        console.error("Event Source error:", event);
      }
    };
    eventSource.current.addEventListener('INIT',  event => {
      const data = JSON.parse(event.data);
      setStatus(data);
      if (data['currentDate']) {
        setCurrentDate(new Date(data['currentDate']));
      }
      timerRef.current = setInterval((offset: number) => {
        setCurrentDate((prevCurrentDate) => new Date(prevCurrentDate.getTime() + 1000));
      }, 1000);
    });
    return () => {
      if (timerRef.current !== -1) {
        clearInterval(timerRef.current);
      }
      setConnected(false);
      setCurrentDate(undefined);
      setStatus(undefined);
      eventSource.current.close();
    }
  }, [ setConnected, setStatus, setCurrentDate ]);

  useEffect(() => {
    const updateEventListener = (event: MessageEvent) => {
      const data = JSON.parse(event.data);
      if (data['currentDate']) {
        setCurrentDate(new Date(data['currentDate']));
      } else {
        setStatus({
          ...status,
          ...data,
        });
      }
    };
    eventSource.current.addEventListener('UPDATE', updateEventListener);
    return () => eventSource.current.removeEventListener('UPDATE', updateEventListener)
  }, [ status, setStatus, setCurrentDate, eventSource.current ]);

  const setWellPumpMode = (mode: 'on' | 'off' | 'auto') => {
    fetch('/api/well-pump', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => console.log(error));
  };

  return (
      <div className="status-main">
        {
          connected
              ? <table className="status-table">
                <tbody>
                <tr>
                  <td>System:</td>
                  <td>
                    <div>
                      {
                        currentDate === undefined
                            ? 'Waiting for NTP response'
                            : currentDate.toLocaleString()
                      }
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Level:</td>
                  <td>
                    <div>
                      {
                        status.waterLevel
                      }%
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Pressure:</td>
                  <td>
                    <div>
                      {
                        status.waterPressure
                      }bar
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Well-P.:</td>
                  <td>
                    <div>
                      <div>
                        {
                          status.wellPump === "active-cycle"
                              ? <div className="led-off led-blinking-yellow"></div>
                              : status.wellPump === "inactive-cycle"
                                  ? <div className="led-off led-flashing-yellow"></div>
                                  : <div className="led-off"></div>
                        }
                        &nbsp;
                        {
                          status.wellPump === "active-cycle"
                              ? `active ${ status.wellPumpCycle === 0 ? '' : `(${status.wellPumpCycle} mins)` }`
                              : status.wellPump === "inactive-cycle"
                                  ? `inactive ${ status.wellPumpCycle === 0 ? '' : `(${status.wellPumpCycle} mins)` }`
                                  : "off"
                        }
                      </div>
                      <div>
                        <button
                            style={ status.wellPumpMode === 'on' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('on') }>
                          1
                        </button>
                        <button
                            style={ status.wellPumpMode === 'auto' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('auto') }>
                          A
                        </button>
                        <button
                            style={ status.wellPumpMode === 'off' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Irrigation-P.:</td>
                  <td>
                    <div>
                      <div>
                        {
                          status.irrigationPump === "active"
                              ? <div className="led-off led-blinking-red"></div>
                              : status.irrigationPump === "out-of-water"
                                  ? <div className="led-off led-blinking-fast-red"></div>
                                  : <div className="led-off"></div>
                        }
                        &nbsp;
                        {
                          status.irrigationPump === "active"
                              ? "active"
                              : status.irrigationPump === "out-of-water"
                                  ? "inactive"
                                  : "off"
                        }
                      </div>
                      <div>
                        <button>1</button>
                        <button>A</button>
                        <button>0</button>
                      </div>
                    </div>
                  </td>
                </tr>
                </tbody>
              </table>
              : <div>Connecting....</div>
        }
      </div>
  );
};

export { Status };
