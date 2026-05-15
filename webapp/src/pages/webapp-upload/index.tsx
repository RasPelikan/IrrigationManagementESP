import { useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { AppContext } from "../../index";
import "./style.css";

const fileKey = (f: File) => ((f as any).webkitRelativePath as string | undefined) || f.name;

const isHidden = (path: string) => path.split('/').some(p => p.startsWith('.'));

const WebappUpload = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  useLayoutEffect(() => {
    setPageTitle('Webapp Upload');
  });

  const [ files, setFiles ] = useState<File[]>([]);
  const [ uploading, setUploading ] = useState(false);
  const [ status, setStatus ] = useState<{ kind: 'ok' | 'error', message: string } | undefined>(undefined);

  // <input webkitdirectory> is non-standard, so set it imperatively to avoid TS friction
  const folderInputRef = useRef<HTMLInputElement>(null);
  useEffect(() => {
    if (folderInputRef.current) {
      folderInputRef.current.setAttribute('webkitdirectory', '');
      folderInputRef.current.setAttribute('directory', '');
    }
  }, []);

  const handleSelect = (event: Event) => {
    const target = event.target as HTMLInputElement;
    const picked = target.files;
    if (!picked) return;
    setFiles(prev => {
      const merged = new Map<string, File>();
      prev.forEach(f => merged.set(fileKey(f), f));
      Array.from(picked)
          .filter(f => !isHidden(fileKey(f)))
          .forEach(f => merged.set(fileKey(f), f));
      return Array.from(merged.values());
    });
    // reset the input so re-selecting the same folder/file fires onChange again
    target.value = '';
  };

  const removeFile = (key: string) => {
    setFiles(prev => prev.filter(f => fileKey(f) !== key));
  };

  const reset = () => {
    setFiles([]);
    setStatus(undefined);
  };

  const upload = () => {
    if (files.length === 0 || uploading) return;
    setStatus(undefined);
    setUploading(true);
    const fd = new FormData();
    // append each file under its basename: the backend places `index.html`/`sw.js` at /www/
    // and everything else under /www/assets/ — it only inspects the basename
    files.forEach(f => fd.append('file', f, f.name));
    fetch('/api/webapp', { method: 'POST', body: fd, redirect: 'manual' })
        .then(res => {
          // backend responds 307 redirect to "/" on success; with redirect:'manual'
          // we get an opaqueredirect response, which is the success signal we want
          if (res.ok || res.type === 'opaqueredirect' || res.status === 307) {
            setStatus({ kind: 'ok', message: `Uploaded ${files.length} file${files.length === 1 ? '' : 's'}. Reload the page to use the new webapp version.` });
            setFiles([]);
          } else {
            setStatus({ kind: 'error', message: `Upload failed: HTTP ${res.status}` });
          }
        })
        .catch(err => setStatus({ kind: 'error', message: `Upload failed: ${err.message}` }))
        .finally(() => setUploading(false));
  };

  return (
      <div className="status-main webappupload-main">
        <p>
          All files of the webapp have to be uploaded at once: <code>index.html</code>, <code>sw.js</code> and everything under <code>assets/</code>.
          Pick the built <code>data/www/</code> folder, or select all files manually.
        </p>
        <div className="webappupload-pickers">
          <label className="webappupload-picker">
            <span>Select folder…</span>
            <input
                ref={ folderInputRef }
                type="file"
                multiple
                onChange={ handleSelect as any }
                disabled={ uploading } />
          </label>
          <label className="webappupload-picker">
            <span>Select files…</span>
            <input
                type="file"
                multiple
                onChange={ handleSelect as any }
                disabled={ uploading } />
          </label>
        </div>
        {
          files.length === 0
              ? <p className="webappupload-empty">No files selected.</p>
              : <ul className="webappupload-list">
                  {
                    files.map(f => {
                      const key = fileKey(f);
                      return (
                          <li key={ key }>
                            <span className="webappupload-name">{ key }</span>
                            <span className="webappupload-size">
                              { (f.size / 1024).toFixed(1) } kB
                            </span>
                            <button
                                type="button"
                                onClick={ () => removeFile(key) }
                                disabled={ uploading }>
                              Remove
                            </button>
                          </li>);
                    })
                  }
                </ul>
        }
        <p>
          <button
              type="button"
              disabled={ files.length === 0 || uploading }
              onClick={ upload }>
            {
              uploading
                  ? 'Uploading…'
                  : files.length === 0
                      ? 'Upload'
                      : `Upload ${files.length} file${files.length === 1 ? '' : 's'}`
            }
          </button>
          &nbsp;
          <button type="button" onClick={ reset } disabled={ uploading || files.length === 0 }>
            Reset
          </button>
        </p>
        {
          status
              ? <p className={ status.kind === 'error' ? 'webappupload-error' : 'webappupload-ok' }>
                  { status.message }
                </p>
              : undefined
        }
      </div>);
};

export { WebappUpload };
