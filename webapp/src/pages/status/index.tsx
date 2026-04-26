import { useCallback, useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";

interface Valve {
  id: string;
  mode: 'on' | 'off' | 'auto';
  active: boolean;
  on: boolean;
}

interface ImStatus {
  error?: string;
  rssi?: number;
  heapAfterSetup?: number;
  heap?: number;
  waterLevel?: number;
  waterPressure?: number;
  waterPressureAdc?: number;
  irrigationPump: 'active' | 'inactive' | 'out-of-water';
  irrigationPumpMode: 'off' | 'auto';
  wellPump: 'active-cycle' | 'inactive-cycle' | 'inactive';
  wellPumpMode: 'on' | 'off' | 'auto';
  wellPumpCycle?: number;
  valves?: Valve[];
}

const Status = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  useLayoutEffect(() => {
    setPageTitle('Status');
  });

  const [ currentDate, setCurrentDate ] = useState<Date | undefined>(undefined);
  const currentDateRef = useRef<number>(0);
  const timerRef = useRef<number>(-1);
  const [ status, setStatus ] = useState<ImStatus>({
    irrigationPump: "inactive",
    irrigationPumpMode: "auto",
    wellPump: "inactive",
    wellPumpMode: "auto",
  });

  const [ pendingWellPump, setPendingWellPump ] = useState(false);
  const [ pendingIrrigationPump, setPendingIrrigationPump ] = useState(false);
  const [ pendingValves, setPendingValves ] = useState<Set<number>>(new Set());

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
        setTimeout(() => eventSource.current = new EventSource('/api/status-events'), 2000);
      } else {
        console.error("Event Source error:", event);
      }
    };
    eventSource.current.addEventListener('INIT',  event => {
      const data = JSON.parse(event.data);
      setStatus(data);
      if (data['currentDate']) {
        currentDateRef.current = new Date(data['currentDate']).getTime();
      }
    });
    timerRef.current = setInterval(() => {
      if (currentDateRef.current === 0) return;
      currentDateRef.current += 1000;
      setCurrentDate(new Date(currentDateRef.current));
    }, 1000);
    return () => {
      if (timerRef.current !== -1) {
        clearInterval(timerRef.current);
      }
      setConnected(false);
      setCurrentDate(undefined);
      setStatus(undefined);
      eventSource.current.close();
    }
  }, [ setConnected, setCurrentDate, setStatus, timerRef ]);

  useEffect(() => {
    const updateEventListener = (event: MessageEvent) => {
      const data = JSON.parse(event.data);
      setStatus({
        ...status,
        ...data
      });
      if (data['currentDate']) {
        currentDateRef.current = new Date(data['currentDate']).getTime();
      }
      if (data['wellPumpMode'] !== undefined) setPendingWellPump(false);
      if (data['irrigationPumpMode'] !== undefined) setPendingIrrigationPump(false);
      if (data['valves'] !== undefined) { setPendingValves(new Set()); setPendingAllValves(false); }
    };
    eventSource.current.addEventListener('UPDATE', updateEventListener);
    return () => eventSource.current.removeEventListener('UPDATE', updateEventListener)
  }, [ status, setStatus, setCurrentDate, eventSource.current ]);

  const setWellPumpMode = (mode: 'on' | 'off' | 'auto') => {
    setPendingWellPump(true);
    fetch('/api/well-pump', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingWellPump(false); });
  };

  const setIrrigationPumpMode = (mode: 'off' | 'auto') => {
    setPendingIrrigationPump(true);
    fetch('/api/irrigation-pump', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingIrrigationPump(false); });
  };

  const setValveMode = (index: number, mode: 'on' | 'off' | 'auto') => {
    setPendingValves(prev => new Set(prev).add(index));
    fetch(`/api/irrigation/valve`, {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `index=${index}&mode=${mode}`
    }).catch(error => { console.log(error); setPendingValves(prev => { const next = new Set(prev); next.delete(index); return next; }); });
  };

  const [ pendingAllValves, setPendingAllValves ] = useState(false);
  const setAllValvesMode = (mode: 'off' | 'auto') => {
    setPendingAllValves(true);
    fetch(`/api/irrigation/valve`, {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingAllValves(false); });
  };

  const freeHeap = status.heapAfterSetup === undefined || status.heap == undefined
      ? undefined
      : status.heapAfterSetup - status.heap;
  return (
      <div className="status-main">
        {
          connected
              ? <table className="status-table">
                <tbody>
                <tr>
                  <td style={ {  verticalAlign: 'top' } }>
                    System:
                  </td>
                  <td>
                    <table>
                    <tbody>
                      <tr>
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
                        <td>
                          <div>
                            RSSI:&nbsp;
                            {
                              status.rssi === undefined
                                  ? 'No RSSI given'
                                  : status.rssi
                            }
                          </div>
                        </td>
                      </tr>
                      <tr>
                        <td>
                          <div>
                            Mem:&nbsp;
                            {
                              freeHeap === undefined
                                  ? 'No heap information'
                                  : <span>{
                                        (freeHeap * 100 / status.heapAfterSetup).toFixed(2)
                                      }% used<br />
                                      ({
                                        freeHeap
                                      } bytes of {
                                        status.heapAfterSetup
                                      })
                                    </span>
                            }
                          </div>
                        </td>
                      </tr>
                      {
                        status.error == undefined
                            ? undefined
                            : <tr>
                                <td style="color: red; font-weight: bold;">
                                  <div>
                                    Error:
                                    {
                                      status.error
                                    }
                                  </div>
                                </td>
                              </tr>
                      }
                    </tbody>
                    </table>
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
                        status.waterPressure?.toFixed(2)
                      }bar (ADC: {
                        status.waterPressureAdc
                      })
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
                            disabled={ pendingWellPump }
                            style={ status.wellPumpMode === 'on' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('on') }>
                          1
                        </button>
                        <button
                            disabled={ pendingWellPump }
                            style={ status.wellPumpMode === 'auto' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingWellPump }
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
                        <button
                            disabled={ pendingIrrigationPump }
                            style={ status.irrigationPumpMode === 'auto'
                                ? { backgroundColor: 'grey', color: 'white' }
                                : undefined }
                            onClick={ () => setIrrigationPumpMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingIrrigationPump }
                            style={ status.irrigationPumpMode === 'off'
                                ? { backgroundColor: 'grey', color: 'white' }
                                : undefined }
                            onClick={ () => setIrrigationPumpMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Valves:</td>
                  <td>
                    <div>
                      <div>
                        All
                      </div>
                      <div>
                        <button
                            disabled={ pendingAllValves }
                            style={ status.valves?.every(v => v.mode === 'auto') ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setAllValvesMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingAllValves }
                            style={ status.valves?.every(v => v.mode === 'off') ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setAllValvesMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td></td>
                  <td>
                    {
                      status.valves?.map((valve, valveIndex) =>
                          <div>
                            <div>
                              {
                                valve.on
                                    ? <div className="led-off led-blinking-blue"></div>
                                    : <div className="led-off led-grey"></div>
                              }
                              &nbsp;
                              {
                                valve.id
                              }
                              :&nbsp;
                              {
                                valve.on
                                    ? "on"
                                    : "off"
                              }
                            </div>
                            <div>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'on' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'on') }>
                                1
                              </button>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'auto' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'auto') }>
                                A
                              </button>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'off' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'off') }>
                                0
                              </button>
                            </div>
                          </div>)
                    }

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
