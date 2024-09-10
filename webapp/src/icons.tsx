const MenuIcon = ({ fillColor }) => {
  return (
      <svg id="hamburger" viewBox="0 0 60 40" style="width: 2rem; height: 2rem;">
        <g stroke={ fillColor } stroke-width="4" stroke-linecap="round" stroke-linejoin="round">
          <path id="top-line" d="M10,10 L50,10 Z"></path>
          <path id="middle-line" d="M10,20 L50,20 Z"></path>
          <path id="bottom-line" d="M10,30 L50,30 Z"></path>
        </g>
      </svg>
  );
}

export {
  MenuIcon
}
