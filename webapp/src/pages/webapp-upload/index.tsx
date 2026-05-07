import { useContext, useEffect, useLayoutEffect, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";

const WebappUpload = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  const [ files, setFiles ] = useState<string[]>([]);

  useLayoutEffect(() => {
    setPageTitle('Webapp Upload');
  });

  const addFile = () => {
    let highest = files.reduce((p, c) => parseInt(c) > p ? parseInt(c) : p, 0);
    setFiles([ ...files, `${highest + 1}` ]);
  };
  const reset = () => {
    setFiles([]);
  };
  const remove = (file: string) => {
    setFiles(files.filter((f) => f !== file));
  };

  return (
      <div className="status-main webappupload-main">
        <p>
          All files of the webapp have to be uploaded at once: <code>index.html</code>, <code>sw.js</code> and everything under <code>assets/</code>.
        </p>
        <form action="/api/webapp" method="POST" enctype="multipart/form-data">
          {
            files.map((file, index) => (
                <label class="file-label" key={ index }>
                  File #{ file }:&nbsp;
                  <input name="{ file }" type="file" />
                  <button type="button" onClick={ () => remove(file) }>Remove</button>
                </label>))
          }
          <button type="button" onClick={ addFile }>Add file</button>
        </form>
        <p>
          <button type="submit" disabled={ files.length === 0 } onClick={ () => document.forms[0].submit() }>Upload</button>
          &nbsp;
          <button type="button" onClick={ reset }>Reset</button>
        </p>
      </div>);
};

export { WebappUpload };
