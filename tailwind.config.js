/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./*.{html,js}", "./src/**/*.{html,js}"],
  theme: {
    extend: {
      fontFamily: {
        flappy: ['FlappyBirdy', 'sans-serif'],
        amd: ['AMD', 'sans-serif'],
        osd: ['OSD', 'sans-serif'],
      },
    },
  },
  plugins: [],
};