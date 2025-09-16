import os from 'node:os';

const getHomeDirectory = () => os.homedir().replace(/\\/g, '/');

export default getHomeDirectory;
