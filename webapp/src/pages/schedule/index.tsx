import { useContext, useEffect, useLayoutEffect, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";

interface ScheduleSequence {
  duration: number;
  startTime?: string;
  valves: string[];
}

interface ScheduleArea {
  name: string;
  reset: boolean;
  irrigatedPeriod: number;
  totalTime: number;
  sequences: ScheduleSequence[];
}

interface ScheduleCycle {
  start: string;
  end: string;
  active: boolean;
  area: ScheduleArea;
}

interface ScheduleData {
  cycles: ScheduleCycle[];
}

const formatTime = (time: string) => {
  return time.substring(0, 2) + ':' + time.substring(2);
};

const Schedule = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  useLayoutEffect(() => {
    setPageTitle('Schedule');
  });

  const [ schedule, setSchedule ] = useState<ScheduleData | undefined>(undefined);
  const [ error, setError ] = useState<string | undefined>(undefined);

  useEffect(() => {
    fetch('/api/irrigation/schedule')
        .then(res => res.json())
        .then(data => setSchedule(data))
        .catch(err => setError(err.message));
  }, []);

  if (error) {
    return <div className="schedule-main">Error: {error}</div>;
  }
  if (!schedule) {
    return <div className="schedule-main">Loading...</div>;
  }

  return (
      <div className="schedule-main">
        {
          schedule.cycles.length === 0
              ? <div>No cycles configured</div>
              : schedule.cycles.map((cycle, i) =>
                  <div className="schedule-cycle" key={i}>
                    <div className="schedule-cycle-header">
                      {
                        cycle.active
                            ? <div className="led-off led-blinking-green"></div>
                            : <div className="led-off led-grey"></div>
                      }
                      &nbsp;
                      {cycle.area.name}
                      <span className="schedule-cycle-time">
                        {formatTime(cycle.start)} - {formatTime(cycle.end)}
                      </span>
                    </div>
                    <div className="schedule-sequences">
                      {
                        cycle.area.sequences.map((seq, j) =>
                            <div className="schedule-sequence" key={j}>
                              <span className="schedule-sequence-time">
                                {seq.startTime ? formatTime(seq.startTime) : '--:--'}
                              </span>
                              <span className="schedule-sequence-valves">
                                {seq.valves.join(', ')}
                              </span>
                              <span className="schedule-sequence-duration">
                                ({seq.duration}min)
                              </span>
                            </div>)
                      }
                    </div>
                  </div>)
        }
      </div>
  );
};

export { Schedule };
