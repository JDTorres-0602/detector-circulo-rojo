// Panel.jsx — Marco holográfico con título
export default function Panel({ titulo, extra, className = '', children }) {
  return (
    <section className={'panel ' + className}>
      {titulo && (
        <header className="panel-cab">
          <span className="panel-titulo">{titulo}</span>
          {extra}
        </header>
      )}
      <div className="panel-cuerpo">{children}</div>
    </section>
  );
}
